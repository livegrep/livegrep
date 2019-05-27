package main

import "github.com/xanzy/go-gitlab"

type gitlabRepoLoader struct {
	client *gitlab.Client
	groups []string
}

func newGitLabRepoLoader(apiURL, token string, groups []string) *gitlabRepoLoader {
	client := gitlab.NewClient(nil, token)
	client.SetBaseURL(apiURL)
	return &gitlabRepoLoader{client: client, groups: groups}
}

func (g *gitlabRepoLoader) loadRepos() ([]repo, error) {
	var out []repo
	for _, groupName := range g.groups {
		projects, _, err := g.client.Groups.ListGroupProjects(groupName, &gitlab.ListGroupProjectsOptions{
			IncludeSubgroups: boolptr(true),
		})
		if err != nil {
			return nil, err
		}

		for _, project := range projects {
			out = append(out, repo{
				name:         project.PathWithNamespace,
				url:          project.WebURL,
				cloneHTTPURL: project.HTTPURLToRepo,
				cloneSSHURL:  project.SSHURLToRepo,
				fork:         false, // No fork-filtering support for GitLab yet
			})
		}
	}

	return out, nil
}

func boolptr(b bool) *bool {
	return &b
}
