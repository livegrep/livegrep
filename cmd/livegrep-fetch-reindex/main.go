package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path"
	"strings"
	"sync"
)

type IndexConfig struct {
	Name         string       `json:"name"`
	Repositories []RepoConfig `json:"repositories"`
}

type RepoConfig struct {
	Path      string            `json:"path"`
	Name      string            `json:"name"`
	Revisions []string          `json:"revisions"`
	Metadata  map[string]string `json:"metadata"`
}

var (
	flagIndexPath   = flag.String("out", "livegrep.idx", "Path to write the index")
	flagRevparse    = flag.Bool("revparse", true, "whether to `git rev-parse` the provided revision in generated links")
	flagSkipMissing = flag.Bool("skip-missing", false, "skip repositories where the specified revision is missing")
)

const Workers = 8

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

	var cfg IndexConfig;
	if err = json.Unmarshal(data, &cfg); err != nil {
		log.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
	}

	err = exec.Command("ssh-add", "-l").Run()
	if err != nil {
		log.Fatalln(err.Error())
	}

	if err := checkoutRepos(&cfg.Repositories); err != nil {
		log.Fatalln(err.Error())
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

	cmd := exec.Command(
		path.Join(path.Dir(path.Dir(path.Dir(os.Args[0]))), "src", "tools", "codesearch"),
		args...,
	)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		log.Fatalln(err)
	}

	if err := os.Rename(tmp, *flagIndexPath); err != nil {
		log.Fatalln("rename:", err.Error())
	}
}

func checkoutRepos(repos *[]RepoConfig) error {
	repoc := make(chan *RepoConfig)
	errc := make(chan error, Workers)
	stop := make(chan struct{})
	wg := sync.WaitGroup{}
	wg.Add(Workers)
	for i := 0; i < Workers; i++ {
		go func() {
			defer wg.Done()
			checkoutWorker(repoc, stop, errc)
		}()
	}

	var err error
Repos:
	for i := range *repos {
		select {
		case repoc <- &(*repos)[i]:
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

func checkoutWorker(c <-chan *RepoConfig,
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

func retryCommand(program string, args []string) error {
	var err error
	for i := 0; i < 3; i++ {
		cmd := exec.Command("git", args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err = cmd.Run(); err == nil {
			return nil
		}
	}
	return fmt.Errorf("%s %v: %s", program, args, err.Error())
}

func checkoutOne(r *RepoConfig) error {
	log.Println("Updating", r.Name)

	remote, ok := r.Metadata["remote"]
	if !ok {
		return fmt.Errorf("git remote not found in repository metadata for %s", r.Name)
	}

	out, err := exec.Command("git", "-C", r.Path, "rev-parse", "--is-bare-repository").Output()
	if err != nil {
		if _, ok := err.(*exec.ExitError); !ok {
			return err
		}
	}
	if strings.Trim(string(out), " \n") != "true" {
		if err := os.RemoveAll(r.Path); err != nil {
			return err
		}
		if err := os.MkdirAll(r.Path, 0755); err != nil {
			return err
		}
		return retryCommand("git", []string{"clone", "--mirror", remote, r.Path})
	}

	if err := exec.Command("git", "-C", r.Path, "remote", "set-url", "origin", remote).Run(); err != nil {
		return err;
	}

	return retryCommand("git", []string{"-C", r.Path, "fetch", "-p"})
}
