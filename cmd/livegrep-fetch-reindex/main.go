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
	"golang.org/x/sync/errgroup"
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
)

// Used to extract the refname from a line like the following:
// ref: refs/heads/good_main_2     HEAD
var remoteHeadRefExtractorReg = regexp.MustCompile("ref:\\s*([^\\s]*)\\s*HEAD")

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

// calls a `git ...` command. Output is printed to stdout/err
func callGit(program string, args []string, username string, password string) error {
	_, err := callGitInternal(program, args, username, password, false)
	return err
}

// calls a `git ...` command. Output is added to a buffer and returned
func callGetGetOutput(program string, args []string, username string, password string) ([]byte, error) {
	buff, err := callGitInternal(program, args, username, password, true)
	return buff, err
}

// calls cmd.Run() if returnOutput is false
// and cmd.Output() otherwise
// always returns an out []byte, but it will always be nil if returnOutput is false
func callGitInternal(program string, args []string, username string, password string, returnOutput bool) ([]byte, error) {
	var err error
	var out []byte

	if username != "" || password != "" {
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
		defer os.Remove(f.Name())

		os.Chmod(f.Name(), 0700)
		args = append([]string{"-c", fmt.Sprintf("credential.helper=%s", f.Name())}, args...)
	}

	for i := 0; i < 3; i++ {
		cmd := exec.Command("git", args...)
		if !returnOutput {
			cmd.Stdout = os.Stdout
			cmd.Stderr = os.Stderr
		}
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
		if !returnOutput {
			err = cmd.Run()
		} else {
			out, err = cmd.Output()
		}
		if err == nil {
			return out, nil
		}
	}
	return nil, fmt.Errorf("%s %v: %s", program, args, err.Error())
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

	// We check and update (if needed) the HEAD ref to avoid scenarios where
	// a remote repo has changed it's head (like a default branch rename/change).
	// git fetch won't do this, at least not on the mirror clones we use.
	// See https://public-inbox.org/git/CANWRddPDhM1g6rtu-a2a=EogXD_hOFwSDsgMCbVvB7dibMaEqw@mail.gmail.com/T/#t
	// for confirmation on this approach from the Git folks.
	//
	// To update the HEAD ref we do the following:
	// 1. Get the remote head ref			- (git ls-remote --symref origin HEAD)
	// 2. Get the current local head ref	- (git symbolic-ref HEAD)
	// 3. Compare them. If outdated, update the local to match remote - (git symbolic-ref HEAD new_ref)
	// We use goroutines to call `git fetch -p` and `git ls-remote --symref origin HEAD` in "parallel"
	// becase they each take ~1.5s.
	var g errgroup.Group

	var remoteOut []byte
	var remoteErr error
	lsRemoteArgs := []string{"--git-dir", r.Path, "ls-remote", "--symref", "origin", "HEAD"}

	// git fetch -p
	g.Go(func() error {
		return callGit("git", args, username, password)
	})

	// git ls-remote --symref origin HEAD
	g.Go(func() error {
		remoteOut, remoteErr = callGetGetOutput("git", lsRemoteArgs, username, password)
		return remoteErr
	})

	if err := g.Wait(); err != nil {
		return err
	}

	currHeadOut, err := exec.Command("git", "--git-dir", r.Path, "symbolic-ref", "HEAD").Output()
	if err != nil {
		return err
	}
	currHead := strings.TrimSpace(string(currHeadOut))

	submatches := remoteHeadRefExtractorReg.FindStringSubmatch(string(remoteOut))
	if len(submatches) == 1 {
		return errors.New("could not parse ls-remote --symref origin HEAD output")
	}
	remoteHead := strings.TrimSpace(submatches[1])

	if currHead == remoteHead { // nothing to do
		return nil
	}

	log.Printf("%s: remote HEAD: %s does not match local HEAD: %s. Attempting to fix...\n", r.Name, remoteHead, currHead)

	// update the HEAD ref
	if err = exec.Command("git", "--git-dir", r.Path, "symbolic-ref", "HEAD", remoteHead).Run(); err != nil {
		log.Printf("%s: error setting symbolic ref. %v\n", r.Name, err)
		return err
	}

	log.Printf("%s: HEAD update done.\n", r.Name)
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
