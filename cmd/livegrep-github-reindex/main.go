package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path"
	"sort"
	"strings"
	"sync"

	"github.com/google/go-github/github"
	"github.com/livegrep/livegrep/src/proto/config"

	"golang.org/x/net/context"
	"golang.org/x/oauth2"
)

const BLDeprecatedMessage = "This flag has been deprecated and will be removed in a future release. Please switch to the '-ignorelist' option."

var (
	flagCodesearch   = flag.String("codesearch", "", "Path to the `codesearch` binary")
	flagFetchReindex = flag.String("fetch-reindex", "", "Path to the `livegrep-fetch-reindex` binary")
	flagApiBaseUrl   = flag.String("api-base-url", "https://api.github.com/", "Github API base url")
	flagGithubKey    = flag.String("github-key", os.Getenv("GITHUB_KEY"), "Github API key")
	flagRepoDir      = flag.String("dir", "repos", "Directory to store repos")
	flagIgnorelist   = flag.String("ignorelist", "", "File containing a list of repositories to ignore when indexing")
	flagDeprecatedBL = flag.String("blacklist", "", "[DEPRECATED] "+BLDeprecatedMessage)
	flagIndexPath    = dynamicDefault{
		display: "${dir}/livegrep.idx",
		fn:      func() string { return path.Join(*flagRepoDir, "livegrep.idx") },
	}
	flagRevision     = flag.String("revision", "HEAD", "git revision to index")
	flagUrlPattern   = flag.String("url-pattern", "https://github.com/{name}/blob/{version}/{path}#L{lno}", "when using the local frontend fileviewer, this string will be used to construt a link to the file source on github")
	flagName         = flag.String("name", "livegrep index", "The name to be stored in the index file")
	flagRevparse     = flag.Bool("revparse", true, "whether to `git rev-parse` the provided revision in generated links")
	flagForks        = flag.Bool("forks", true, "whether to index repositories that are github forks, and not original repos")
	flagArchived     = flag.Bool("archived", false, "whether to index repositories that are archived on github")
	flagHTTP         = flag.Bool("http", false, "clone repositories over HTTPS instead of SSH")
	flagHTTPUsername = flag.String("http-user", "git", "Override the username to use when cloning over https")
	flagInstallation = flag.Bool("installation-token", false, "Treat the API key as a Github Application Installation Key when cloning")
	flagDepth        = flag.Int("depth", 0, "clone repository with specify --depth=N depth.")
	flagSkipMissing  = flag.Bool("skip-missing", false, "skip repositories where the specified revision is missing")
	flagRepos        = stringList{}
	flagOrgs         = stringList{}
	flagUsers        = stringList{}
)

func init() {
	flag.Var(&flagIndexPath, "out", "Path to write the index")
	flag.Var(&flagRepos, "repo", "Specify a repo to index (may be passed multiple times)")
	flag.Var(&flagOrgs, "org", "Specify a github organization to index (may be passed multiple times)")
	flag.Var(&flagUsers, "user", "Specify a github user to index (may be passed multiple times)")
}

const Workers = 8

func main() {
	flag.Parse()
	log.SetFlags(0)

	if *flagDeprecatedBL != "" {
		log.Fatalln(BLDeprecatedMessage)
	}

	if flagRepos.strings == nil &&
		flagOrgs.strings == nil &&
		flagUsers.strings == nil {
		log.Fatal("You must specify at least one repo or organization to index")
	}

	if *flagInstallation {
		if *flagGithubKey == "" {
			log.Fatal("-installation-key requires passing a github key, via either -github-key or $GITHUB_KEY")
		}
		*flagHTTP = true
		*flagHTTPUsername = "x-access-token"
	}

	var ignorelist map[string]struct{}
	if *flagIgnorelist != "" {
		var err error
		ignorelist, err = loadIgnorelist(*flagIgnorelist)
		if err != nil {
			log.Fatalf("loading %s: %s", *flagIgnorelist, err)
		}
	}

	var h *http.Client
	if *flagGithubKey == "" {
		h = http.DefaultClient
	} else {
		tok := &oauth2.Token{AccessToken: *flagGithubKey}
		h = oauth2.NewClient(
			context.Background(),
			oauth2.StaticTokenSource(tok),
		)
	}

	gh := github.NewClient(h)

	if *flagApiBaseUrl != "" {
		if !strings.HasSuffix(*flagApiBaseUrl, "/") {
			log.Fatalf("API base URL must include trailing slash: %s", *flagApiBaseUrl)
		}
		baseURL, err := url.Parse(*flagApiBaseUrl)
		if err != nil {
			log.Fatalf("parsing base url %s: %v", *flagApiBaseUrl, err)
		}
		gh.BaseURL = baseURL
	}

	repos, err := loadRepos(gh,
		flagRepos.strings,
		flagOrgs.strings,
		flagUsers.strings)
	if err != nil {
		log.Fatalln(err.Error())
	}

	repos = filterRepos(repos, ignorelist, !*flagForks, !*flagArchived)

	sort.Sort(ReposByName(repos))

	config, err := buildConfig(*flagName, *flagRepoDir, repos, *flagRevision)
	if err != nil {
		log.Fatalln(err.Error())
	}
	configPath := path.Join(*flagRepoDir, "livegrep.json")
	if err := writeConfig(config, configPath); err != nil {
		log.Fatalln(err.Error())
	}

	index := flagIndexPath.Get().(string)

	args := []string{
		"--out", index,
		"--codesearch", *flagCodesearch,
	}
	if *flagRevparse {
		args = append(args, "--revparse")
	}
	if *flagSkipMissing {
		args = append(args, "--skip-missing")
	}
	args = append(args, configPath)

	if *flagFetchReindex == "" {
		fr := findBinary("livegrep-fetch-reindex")
		flagFetchReindex = &fr
	}

	log.Printf("Running: %s %v\n", *flagFetchReindex, args)
	cmd := exec.Command(*flagFetchReindex, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if *flagGithubKey != "" {
		cmd.Env = append(os.Environ(), fmt.Sprintf("GITHUB_KEY=%s", *flagGithubKey))
	}
	if err := cmd.Run(); err != nil {
		log.Fatalln("livegrep-fetch-reindex: ", err)
	}
}

func findBinary(name string) string {
	paths := []string{
		path.Join(path.Dir(os.Args[0]), name),
		strings.Replace(os.Args[0], path.Base(os.Args[0]), name, -1),
	}
	for _, try := range paths {
		if st, err := os.Stat(try); err == nil && (st.Mode()&os.ModeDir) == 0 {
			return try
		}
	}
	return name
}

type ReposByName []*github.Repository

func (r ReposByName) Len() int           { return len(r) }
func (r ReposByName) Swap(i, j int)      { r[i], r[j] = r[j], r[i] }
func (r ReposByName) Less(i, j int) bool { return *r[i].FullName < *r[j].FullName }

func loadIgnorelist(path string) (map[string]struct{}, error) {
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

type loadJob struct {
	obj string
	get func(*github.Client, string) ([]*github.Repository, error)
}

type maybeRepo struct {
	repos []*github.Repository
	err   error
}

func loadRepos(
	client *github.Client,
	repos []string,
	orgs []string,
	users []string) ([]*github.Repository, error) {

	jobc := make(chan loadJob)
	done := make(chan struct{})
	repoc := make(chan maybeRepo)

	var jobs []loadJob
	for _, repo := range repos {
		jobs = append(jobs, loadJob{repo, getOneRepo})
	}
	for _, org := range orgs {
		jobs = append(jobs, loadJob{org, getOrgRepos})
	}
	for _, user := range users {
		jobs = append(jobs, loadJob{user, getUserRepos})
	}
	go func() {
		defer close(jobc)
		for _, j := range jobs {
			select {
			case jobc <- j:
			case <-done:
				return
			}
		}
	}()
	var wg sync.WaitGroup
	wg.Add(Workers)
	for i := 0; i < Workers; i++ {
		go func() {
			runJobs(client, jobc, done, repoc)
			wg.Done()
		}()
	}
	go func() {
		wg.Wait()
		close(repoc)
	}()
	var out []*github.Repository
	for repo := range repoc {
		if repo.err != nil {
			close(done)
			return nil, repo.err
		}
		out = append(out, repo.repos...)
	}

	return out, nil
}

func runJobs(client *github.Client, jobc <-chan loadJob, done <-chan struct{}, out chan<- maybeRepo) {
	for {
		var job loadJob
		var ok bool
		select {
		case job, ok = <-jobc:
			if !ok {
				return
			}
		case <-done:
			return
		}
		var res maybeRepo
		res.repos, res.err = job.get(client, job.obj)
		select {
		case out <- res:
		case <-done:
			return
		}
	}
}

func filterRepos(repos []*github.Repository,
	ignorelist map[string]struct{},
	excludeForks bool, excludeArchived bool) []*github.Repository {
	var out []*github.Repository

	for _, r := range repos {
		if excludeForks && r.Fork != nil && *r.Fork {
			log.Printf("Excluding fork %s...", *r.FullName)
			continue
		}
		if excludeArchived && r.Archived != nil && *r.Archived {
			log.Printf("Excluding archived %s...", *r.FullName)
			continue
		}
		if ignorelist != nil {
			if _, ok := ignorelist[*r.FullName]; ok {
				continue
			}
		}
		out = append(out, r)
	}

	return out
}

func getOneRepo(client *github.Client, repo string) ([]*github.Repository, error) {
	bits := strings.SplitN(repo, "/", 2)
	if len(bits) != 2 {
		return nil, fmt.Errorf("Bad repository: %s", repo)
	}

	ghRepo, _, err := client.Repositories.Get(context.TODO(), bits[0], bits[1])
	if err != nil {
		return nil, err
	}
	return []*github.Repository{ghRepo}, nil
}

func getOrgRepos(client *github.Client, org string) ([]*github.Repository, error) {
	var buf []*github.Repository
	opt := &github.RepositoryListByOrgOptions{
		ListOptions: github.ListOptions{PerPage: 50},
	}
	for {
		repos, resp, err := client.Repositories.ListByOrg(context.TODO(), org, opt)
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

func getUserRepos(client *github.Client, user string) ([]*github.Repository, error) {
	var buf []*github.Repository
	opt := &github.RepositoryListOptions{
		ListOptions: github.ListOptions{PerPage: 50},
	}
	for {
		repos, resp, err := client.Repositories.List(context.TODO(), user, opt)
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

func writeConfig(config []byte, file string) error {
	dir := path.Dir(file)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}
	return ioutil.WriteFile(file, config, 0644)
}

func buildConfig(name string,
	dir string,
	repos []*github.Repository,
	revision string) ([]byte, error) {
	cfg := config.IndexSpec{
		Name: name,
	}

	for _, r := range repos {
		if *flagSkipMissing {
			cmd := exec.Command("git",
				"--git-dir",
				path.Join(dir, *r.FullName),
				"rev-parse",
				"--verify",
				revision,
			)
			if e := cmd.Run(); e != nil {
				log.Printf("Skipping missing revision repo=%s rev=%s",
					*r.FullName, revision,
				)
				continue
			}
		}
		var remote string
		if *flagHTTP {
			remote = *r.CloneURL
		} else {
			remote = *r.SSHURL
		}

		var password_env string
		if *flagGithubKey != "" {
			password_env = "GITHUB_KEY"
		}

		cfg.Repositories = append(cfg.Repositories, &config.RepoSpec{
			Path:      path.Join(dir, *r.FullName),
			Name:      *r.FullName,
			Revisions: []string{revision},
			Metadata: &config.Metadata{
				Github:     *r.HTMLURL,
				Remote:     remote,
				UrlPattern: *flagUrlPattern,
			},
			CloneOptions: &config.CloneOptions{
				Depth:       int32(*flagDepth),
				Username:    *flagHTTPUsername,
				PasswordEnv: password_env,
			},
		})
	}

	return json.MarshalIndent(cfg, "", "  ")
}
