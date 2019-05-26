package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"strings"
	"sync"

	"github.com/google/go-github/github"
	"golang.org/x/oauth2"
)

type githubRepoLoader struct {
	client             *github.Client
	repos, users, orgs []string
}

func newGitHubRepoLoader(
	githubBaseURL, githubKey string,
	repos, users, orgs []string,
) *githubRepoLoader {
	var h *http.Client
	if githubKey == "" {
		h = http.DefaultClient
	} else {
		tok := &oauth2.Token{AccessToken: githubKey}
		h = oauth2.NewClient(
			context.Background(),
			oauth2.StaticTokenSource(tok),
		)
	}

	gh := github.NewClient(h)

	if githubBaseURL != "" {
		if !strings.HasSuffix(githubBaseURL, "/") {
			log.Fatalf("API base URL must include trailing slash: %s", githubBaseURL)
		}
		baseURL, err := url.Parse(githubBaseURL)
		if err != nil {
			log.Fatalf("parsing base url %s: %v", githubBaseURL, err)
		}
		gh.BaseURL = baseURL
	}

	return &githubRepoLoader{
		client: gh,
		repos:  repos, users: users, orgs: orgs,
	}
}

func (g *githubRepoLoader) loadRepos() ([]repo, error) {
	jobc := make(chan loadJob)
	done := make(chan struct{})
	repoc := make(chan maybeRepo)

	var jobs []loadJob
	for _, repo := range g.repos {
		jobs = append(jobs, loadJob{repo, getOneRepo})
	}
	for _, org := range g.orgs {
		jobs = append(jobs, loadJob{org, getOrgRepos})
	}
	for _, user := range g.users {
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
			runJobs(g.client, jobc, done, repoc)
			wg.Done()
		}()
	}
	go func() {
		wg.Wait()
		close(repoc)
	}()
	var out []repo
	for r := range repoc {
		if r.err != nil {
			close(done)
			return nil, r.err
		}
		for _, innerRepo := range r.repos {
			out = append(out, repo{
				name:         *innerRepo.FullName,
				url:          *innerRepo.HTMLURL,
				cloneHTTPURL: *innerRepo.CloneURL,
				cloneSSHURL:  *innerRepo.SSHURL,
				fork:         innerRepo.Fork != nil && *innerRepo.Fork,
			})
		}
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
