package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path"
	"regexp"
	"strings"
	"sync"

	"github.com/livegrep/livegrep/src/proto/config"
	pb "github.com/livegrep/livegrep/src/proto/go_proto"
	"google.golang.org/grpc"
)

var (
	flagCodesearch    = flag.String("codesearch", "", "Path to the `codesearch` binary")
	flagIndexPath     = flag.String("out", "livegrep.idx", "Path to write the index")
	flagRevparse      = flag.Bool("revparse", true, "whether to `git rev-parse` the provided revision in generated links")
	flagSkipMissing   = flag.Bool("skip-missing", false, "skip repositories where the specified revision is missing")
	flagReloadBackend = flag.String("reload-backend", "", "Backend to send a Reload RPC to")
	flagNumWorkers    = flag.Int("num-workers", 8, "Number of workers used to update repositories")
	flagNoIndex       = flag.Bool("no-index", false, "Skip indexing after fetching")
	flagMetricsPath   = flag.String("metrics-out", "", "File that indexing metrics should be sent to")
	flagStatsdOn      = flag.Bool("send-metrics-to-statsd", false, "Send indexing metrics to StatsD")
	flagStatsdAddr    = flag.String("statsd-address", "", "address URI of statsd listener for metrics export")
	flagStatsdPrefix  = flag.String("statsd-prefix", "", "optional prefix to apply to all metrics")
	flagUpdateHead    = flag.Bool("update-head", false, "update the HEAD reference if it has changed on remote")
)

var (
	// Uses to extract the refname from a line like the following:
	// ref: refs/heads/good_main_2     HEAD
	remoteHeadRefExtractorReg = regexp.MustCompile("ref:\\s*([^\\s]*)\\s*HEAD")
)

func main() {
	flag.Parse()
	log.SetFlags(0)

	if len(flag.Args()) != 1 {
		log.Fatal("Expected exactly one argument (the index json configuration)")
	}

	data, err := ioutil.ReadFile(flag.Arg(0))
	if err != nil {
		log.Fatalf(err.Error())
	}

	var cfg config.IndexSpec
	if err = json.Unmarshal(data, &cfg); err != nil {
		log.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
	}

	if err := checkoutRepos(&cfg.Repositories); err != nil {
		log.Fatalln(err.Error())
	}

	if *flagNoIndex {
		log.Printf("Skipping indexing after fetching repos")
		return
	}

	tmp := *flagIndexPath + ".tmp"

	args := []string{
		"--debug=ui",
		"--dump_index",
		tmp,
		"--index_only",
	}
	if *flagRevparse {
		args = append(args, "--revparse")
	}
	args = append(args, flag.Arg(0))

	cmd := exec.Command(findCodesearch(*flagCodesearch), args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		log.Fatalln(err)
	}

	if err := os.Rename(tmp, *flagIndexPath); err != nil {
		log.Fatalln("rename:", err.Error())
	}

	if *flagReloadBackend != "" {
		if err := reloadBackend(*flagReloadBackend); err != nil {
			log.Fatalln("reload:", err.Error())
		}
	}
}

func findCodesearch(given string) string {
	if given != "" {
		return given
	}
	search := []string{
		path.Join(path.Dir(os.Args[0]), "codesearch"),
		"bazel-bin/src/tools/codesearch",
	}
	for _, try := range search {
		if st, err := os.Stat(try); err == nil && (st.Mode()&os.ModeDir) == 0 {
			return try
		}
	}
	return "codesearch"
}

func checkoutRepos(repos *[]*config.RepoSpec) error {
	repoc := make(chan *config.RepoSpec)
	errc := make(chan error, *flagNumWorkers)
	stop := make(chan struct{})
	wg := sync.WaitGroup{}
	wg.Add(*flagNumWorkers)
	for i := 0; i < *flagNumWorkers; i++ {
		go func() {
			defer wg.Done()
			checkoutWorker(repoc, stop, errc)
		}()
	}

	var err error
Repos:
	for i := range *repos {
		select {
		case repoc <- (*repos)[i]:
		case err = <-errc:
			close(stop)
			break Repos
		}
	}

	close(repoc)
	wg.Wait()
	select {
	case err = <-errc:
	default:
	}

	return err
}

func checkoutWorker(c <-chan *config.RepoSpec,
	stop <-chan struct{}, errc chan error) {
	for {
		select {
		case r, ok := <-c:
			if !ok {
				return
			}
			err := checkoutOne(r)
			if err != nil {
				errc <- err
			}
		case <-stop:
			return
		}
	}
}

const credentialHelperScript = (`#!/bin/sh
if test "$1" = "get"; then
  pass=` + "`cat <&3`" + `
  if test -n "$LIVEGREP_GITHUB_USERNAME"; then
    echo "username=$LIVEGREP_GITHUB_USERNAME"
  fi
  if test -n "$pass"; then
    echo "password=$pass"
  fi
fi
`)

// runs exec.Output() and returns the command output
func callGit2(program string, args []string, username string, password string) ([]byte, error) {
	var err error
	var out []byte

	if username != "" || password != "" {
		f, err := passwordHelper(username, password, &args)
		if err != nil {
			return nil, err
		}
		defer os.Remove(f.Name())
	}

	for i := 0; i < 3; i++ {
		cmd := exec.Command("git", args...)
		if password != "" {
			r, w, err := os.Pipe()
			if err != nil {
				return nil, err
			}

			cmd.ExtraFiles = []*os.File{r}

			go func() {
				defer w.Close()
				w.WriteString(password)
			}()
			defer r.Close()
		}
		if username != "" {
			cmd.Env = append(os.Environ(), fmt.Sprintf("LIVEGREP_GITHUB_USERNAME=%s", username))
		}
		out, err = cmd.Output()
		if err == nil {
			return out, nil
		}
	}
	return nil, fmt.Errorf("%s %v: %s", program, args, err.Error())
}

func passwordHelper(username string, password string, args *[]string) (*os.File, error) {
	// If we're given credentials, pass them to git via a
	// credential helper
	//
	// In order to avoid the key hitting the
	// filesystem, we pass the actual key via a
	// pipe on fd `3`, and we set the askpass
	// script to a tiny sh script that just reads
	// from that pipe.
	f, err := ioutil.TempFile("", "livegrep-credential-helper")
	if err != nil {
		return nil, err
	}
	f.WriteString(credentialHelperScript)
	f.Close()

	os.Chmod(f.Name(), 0700)
	*args = append([]string{"-c", fmt.Sprintf("credential.helper=%s", f.Name())}, *args...)

	return f, nil
}

func callGit(program string, args []string, username string, password string) error {
	var err error

	if username != "" || password != "" {
		f, err := passwordHelper(username, password, &args)
		if err != nil {
			return err
		}
		defer os.Remove(f.Name())
	}

	for i := 0; i < 3; i++ {
		cmd := exec.Command("git", args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if password != "" {
			r, w, err := os.Pipe()
			if err != nil {
				return err
			}

			cmd.ExtraFiles = []*os.File{r}

			go func() {
				defer w.Close()
				w.WriteString(password)
			}()
			defer r.Close()
		}
		if username != "" {
			cmd.Env = append(os.Environ(), fmt.Sprintf("LIVEGREP_GITHUB_USERNAME=%s", username))
		}
		if err = cmd.Run(); err == nil {
			return nil
		}
	}
	return fmt.Errorf("%s %v: %s", program, args, err.Error())
}

func checkoutOne(r *config.RepoSpec) error {
	log.Println("Updating", r.Name)

	remote := r.Metadata.Remote
	if remote == "" {
		return fmt.Errorf("git remote not found in repository metadata for %s", r.Name)
	}

	out, err := exec.Command("git", "-C", r.Path, "rev-parse", "--is-bare-repository").Output()
	if err != nil {
		if _, ok := err.(*exec.ExitError); !ok {
			return err
		}
	}
	var username, password string
	if r.CloneOptions != nil {
		username = r.CloneOptions.Username
		password = os.Getenv(r.CloneOptions.PasswordEnv)
	}
	if strings.Trim(string(out), " \n") != "true" {
		if err := os.RemoveAll(r.Path); err != nil {
			return err
		}
		if err := os.MkdirAll(r.Path, 0755); err != nil {
			return err
		}
		args := []string{"clone", "--mirror"}
		if r.CloneOptions != nil && r.CloneOptions.Depth != 0 {
			args = append(args, fmt.Sprintf("--depth=%d", r.CloneOptions.Depth))
		}
		args = append(args, remote, r.Path)
		return callGit("git", args, username, password)
	}

	if err := exec.Command("git", "-C", r.Path, "remote", "set-url", "origin", remote).Run(); err != nil {
		return err
	}

	args := []string{"--git-dir", r.Path, "fetch", "-p"}
	if r.CloneOptions != nil && r.CloneOptions.Depth != 0 {
		args = append(args, fmt.Sprintf("--depth=%d", r.CloneOptions.Depth))
	}
	err = callGit("git", args, username, password)
	if err != nil {
		return err
	}

	if !(*flagUpdateHead) {
		return nil
	}

	// We check and update (if needed) the HEAD ref to avoid scenarios where
	// a remote repo has changed it's head (like a default branch rename/change).
	// git fetch won't do this, at least not on the mirror clones we use.
	// See https://public-inbox.org/git/CANWRddPDhM1g6rtu-a2a=EogXD_hOFwSDsgMCbVvB7dibMaEqw@mail.gmail.com/T/#t
	// for confirmation on this approach from the Git folks.
	//
	// To update the HEAD ref we do the following:
	// 1. Get the current local head ref	- (git symbolic-ref HEAD)
	// 2. Get the remote head ref			- (git ls-remote --symref origin HEAD)
	// 3. Compare them. If outdated, update the local to match remote - (git symbolic-ref HEAD new_ref)
	// The whole process takes around ~1.5s per repo, so if you have many repos to update you may want to increase
	// the number of workers, or only use the -update-head option periodically.
	currHeadOut, err := exec.Command("git", "--git-dir", r.Path, "symbolic-ref", "HEAD").Output()
	if err != nil {
		return err
	}
	currHead := strings.TrimSpace(string(currHeadOut))

	args2 := []string{"--git-dir", r.Path, "ls-remote", "--symref", "origin", "HEAD"}
	remoteOut, err := callGit2("git", args2, username, password)
	if err != nil {
		return err
	}

	submatches := remoteHeadRefExtractorReg.FindStringSubmatch(string(remoteOut))
	if len(submatches) == 1 {
		return errors.New("could not parse ls-remote --symref origin HEAD output")
	}
	remoteHead := strings.TrimSpace(submatches[1])

	if currHead != remoteHead {
		log.Printf("%s: remote HEAD: %s does not match local HEAD: %s. Attempting to fix...\n", r.Name, remoteHead, currHead)

		// update the HEAD ref
		err = exec.Command("git", "--git-dir", r.Path, "symbolic-ref", "HEAD", remoteHead).Run()
		if err != nil {
			log.Printf("error setting symbolic ref. %v\n", err)
			return err
		}

		log.Printf("%s: HEAD update done.\n")
	}

	return nil
}

func reloadBackend(addr string) error {
	client, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		return err
	}

	codesearch := pb.NewCodeSearchClient(client)

	if _, err = codesearch.Reload(context.Background(), &pb.Empty{}, grpc.FailFast(false)); err != nil {
		return err
	}
	return nil
}
