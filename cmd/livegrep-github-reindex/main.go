package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path"
	"sort"
	"strings"
	"sync"

	"github.com/google/go-github/github"

	"code.google.com/p/goauth2/oauth"
)

var (
	flagGithubKey = flag.String("github-key", os.Getenv("GITHUB_KEY"), "Github API key")
	flagRepoDir   = flag.String("dir", "repos", "Directory to store repos")
	flagBlacklist = flag.String("blacklist", "", "File containing a list of repositories to blacklist indexing")
	flagIndexPath = dynamicDefault{
		display: "${dir}/livegrep.idx",
		fn:      func() string { return path.Join(*flagRepoDir, "livegrep.idx") },
	}
	flagName  = flag.String("name", "livegrep index", "The name to be stored in the index file")
	flagForks = flag.Bool("forks", true, "whether to index repositories that are github forks, and not original repos")
	flagRepos = stringList{}
	flagOrgs  = stringList{}
	flagUsers = stringList{}
)

func init() {
	flag.Var(&flagIndexPath, "out", "Path to write the index")
	flag.Var(&flagRepos, "repo", "Specify a repo to index (may be passed multiple times)")
	flag.Var(&flagOrgs, "org", "Specify a github organization to index (may be passed multiple times)")
	flag.Var(&flagUsers, "user", "Specify a github user to index (may be passed multiple times)")
}

func main() {
	flag.Parse()
	log.SetFlags(0)

	if flagRepos.strings == nil &&
		flagOrgs.strings == nil &&
		flagUsers.strings == nil {
		log.Fatal("You must specify at least one repo or organization to index")
	}

	var blacklist map[string]struct{}
	if *flagBlacklist != "" {
		var err error
		blacklist, err = loadBlacklist(*flagBlacklist)
		if err != nil {
			log.Fatalf("loading %s: %s", *flagBlacklist, err)
		}
	}

	var h *http.Client
	if *flagGithubKey == "" {
		h = http.DefaultClient
	} else {
		t := &oauth.Transport{
			Token: &oauth.Token{AccessToken: *flagGithubKey},
		}
		h = t.Client()
	}

	gh := github.NewClient(h)

	repos, err := loadRepos(gh,
		flagRepos.strings,
		flagOrgs.strings,
		flagUsers.strings)
	if err != nil {
		log.Fatalln(err.Error())
	}

	repos = filterRepos(repos, blacklist, !*flagForks)

	sort.Sort(ReposByName(repos))

	if err := checkoutRepos(repos, *flagRepoDir); err != nil {
		log.Fatalln(err.Error())
	}

	if err := writeConfig(*flagName, repos, *flagRepoDir); err != nil {
		log.Fatalln(err.Error())
	}

	index := flagIndexPath.Get().(string)
	tmp := index + ".tmp"

	cmd := exec.Command(
		path.Join(path.Dir(os.Args[0]), "codesearch"),
		"--debug=ui",
		"--dump_index",
		tmp,
		"--revparse",
		path.Join(*flagRepoDir, "livegrep.json"))
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		log.Fatalln(err)
	}

	if err := os.Rename(tmp, index); err != nil {
		log.Fatalln("rename:", err.Error())
	}
}

type ReposByName []github.Repository

func (r ReposByName) Len() int           { return len(r) }
func (r ReposByName) Swap(i, j int)      { r[i], r[j] = r[j], r[i] }
func (r ReposByName) Less(i, j int) bool { return *r[i].FullName < *r[j].FullName }

func loadBlacklist(path string) (map[string]struct{}, error) {
	data, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, err
	}

	lines := strings.Split(string(data), "\n")
	out := make(map[string]struct{}, len(lines))
	for _, l := range lines {
		out[l] = struct{}{}
	}
	return out, nil
}

func loadRepos(
	client *github.Client,
	repos []string,
	orgs []string,
	users []string) ([]github.Repository, error) {
	var out []github.Repository
	for _, repo := range repos {
		bits := strings.SplitN(repo, "/", 2)
		if len(bits) != 2 {
			return nil, fmt.Errorf("Bad repository: %s", repo)
		}

		ghRepo, _, err := client.Repositories.Get(bits[0], bits[1])
		if err != nil {
			return nil, err
		}
		out = append(out, *ghRepo)
	}

	for _, org := range orgs {
		var err error
		log.Printf("fetching repos in org %s...", org)
		out, err = listOrgRepos(client, org, out)
		if err != nil {
			log.Fatalf("listing %s: %s", org, err.Error())
		}
	}

	for _, user := range users {
		var err error
		log.Printf("fetching repos for user %s...", user)
		out, err = listUserRepos(client, user, out)
		if err != nil {
			log.Fatalf("listing %s: %s", user, err.Error())
		}
	}

	return out, nil
}

func filterRepos(repos []github.Repository,
	blacklist map[string]struct{},
	excludeForks bool) []github.Repository {
	var out []github.Repository

	for _, r := range repos {
		if excludeForks && r.Fork != nil && *r.Fork {
			log.Printf("Excluding fork %s...", *r.FullName)
			continue
		}
		if blacklist != nil {
			if _, ok := blacklist[*r.FullName]; ok {
				continue
			}
		}
		out = append(out, r)
	}

	return out
}

func listOrgRepos(client *github.Client, org string, buf []github.Repository) ([]github.Repository, error) {
	opt := &github.RepositoryListByOrgOptions{
		ListOptions: github.ListOptions{PerPage: 50},
	}
	for {
		repos, resp, err := client.Repositories.ListByOrg(org, opt)
		if err != nil {
			return nil, err
		}
		buf = append(buf, repos...)
		if resp.NextPage == 0 {
			break
		}
		opt.ListOptions.Page = resp.NextPage
	}
	return buf, nil
}

func listUserRepos(client *github.Client, user string, buf []github.Repository) ([]github.Repository, error) {
	opt := &github.RepositoryListOptions{
		ListOptions: github.ListOptions{PerPage: 50},
	}
	for {
		repos, resp, err := client.Repositories.List(user, opt)
		if err != nil {
			return nil, err
		}
		buf = append(buf, repos...)
		if resp.NextPage == 0 {
			break
		}
		opt.ListOptions.Page = resp.NextPage
	}
	return buf, nil
}

const Workers = 8

func checkoutRepos(repos []github.Repository, dir string) error {
	repoc := make(chan *github.Repository)
	errc := make(chan error, Workers)
	stop := make(chan struct{})
	wg := sync.WaitGroup{}
	wg.Add(Workers)
	for i := 0; i < Workers; i++ {
		go func() {
			defer wg.Done()
			checkoutWorker(dir, repoc, stop, errc)
		}()
	}

	var err error
Repos:
	for i := range repos {
		select {
		case repoc <- &repos[i]:
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

func checkoutWorker(dir string,
	c <-chan *github.Repository,
	stop <-chan struct{}, errc chan error) {
	for {
		select {
		case r, ok := <-c:
			if !ok {
				return
			}
			err := checkoutOne(dir, r)
			if err != nil {
				errc <- err
			}
		case <-stop:
			return
		}
	}
}

func checkoutOne(dir string, r *github.Repository) error {
	log.Println("Updating", *r.FullName)
	checkout := path.Join(dir, *r.FullName)
	out, err := exec.Command("git", "--git-dir", checkout, "rev-parse", "--is-bare-repository").Output()
	if err != nil {
		if _, ok := err.(*exec.ExitError); !ok {
			return err
		}
	}
	if strings.Trim(string(out), " \n") != "true" {
		if err := os.RemoveAll(checkout); err != nil {
			return err
		}
		if err := os.MkdirAll(checkout, 0755); err != nil {
			return err
		}
		cmd := exec.Command("git", "clone", "--mirror", *r.SSHURL, checkout)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return err
		}
	}

	cmd := exec.Command("git", "--git-dir", checkout, "fetch", "-p")
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return err
	}
	return nil
}

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

func writeConfig(name string, repos []github.Repository, dir string) error {
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	cfg := IndexConfig{
		Name: name,
	}

	for _, r := range repos {
		cfg.Repositories = append(cfg.Repositories, RepoConfig{
			Path:      path.Join(dir, *r.FullName),
			Name:      *r.FullName,
			Revisions: []string{"HEAD"},
			Metadata: map[string]string{
				"github": *r.FullName,
			},
		})
	}

	data, err := json.MarshalIndent(cfg, "", "  ")
	if err != nil {
		return err
	}

	return ioutil.WriteFile(path.Join(dir, "livegrep.json"),
		data, 0644)
}
