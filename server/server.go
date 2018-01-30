package server

import (
	"fmt"
	"html/template"
	"io"
	stdlog "log"
	"net/http"
	"path"
	"strings"
	texttemplate "text/template"
	"time"

	"golang.org/x/net/context"

	"github.com/bmizerany/pat"
	libhoney "github.com/honeycombio/libhoney-go"

	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/livegrep/livegrep/server/reqid"
	"github.com/livegrep/livegrep/server/templates"

)

type Templates struct {
	Layout,
	Index,
	FileView,
	BlameDiff,
	BlameFile,
	About *template.Template
	OpenSearch *texttemplate.Template `template:"opensearch.xml"`
}

type server struct {
	config      *config.Config
	bk          map[string]*Backend
	bkOrder     []string
	repos       map[string]config.RepoConfig
	inner       http.Handler
	T           Templates
	AssetHashes map[string]string
	Layout      *template.Template

	honey *libhoney.Builder
}

func (s *server) loadTemplates() {
	s.AssetHashes = make(map[string]string)
	err := templates.Load(
		path.Join(s.config.DocRoot, "templates"),
		&s.T,
		path.Join(s.config.DocRoot, "hashes.txt"),
		s.AssetHashes)
	if err != nil {
		panic(fmt.Sprintf("loading templates: %v", err))
	}
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.inner.ServeHTTP(w, r)
}

func (s *server) ServeSearch(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	urls := make(map[string]map[string]string, len(s.bk))
	backends := make([]*Backend, 0, len(s.bk))
	sampleRepo := ""
	for _, bkId := range s.bkOrder {
		bk := s.bk[bkId]
		backends = append(backends, bk)
		bk.I.Lock()
		m := make(map[string]string, len(bk.I.Trees))
		urls[bk.Id] = m
		for _, r := range bk.I.Trees {
			if sampleRepo == "" {
				sampleRepo = r.Name
			}
			m[r.Name] = r.Url
		}
		bk.I.Unlock()
	}
	page_data := &struct {
		Backends   []*Backend
		SampleRepo string
	}{backends, sampleRepo}
	script_data := &struct {
		RepoUrls           map[string]map[string]string `json:"repo_urls"`
		InternalViewRepos  map[string]config.RepoConfig `json:"internal_view_repos"`
		DefaultSearchRepos []string                     `json:"default_search_repos"`
	}{urls, s.repos, s.config.DefaultSearchRepos}

	body, err := executeTemplate(s.T.Index, page_data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         "code search",
		ScriptName:    "codesearch",
		ScriptData:    script_data,
		IncludeHeader: true,
		Body:          template.HTML(body),
	})
}

func (s *server) ServeFile(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	repoName := r.URL.Query().Get(":repo")
	path := pat.Tail("/view/:repo/", r.URL.Path)
	commit := r.URL.Query().Get("commit")
	if commit == "" {
		commit = "HEAD"
	}

	if len(s.repos) == 0 {
		http.Error(w, "File browsing not enabled", 404)
		return
	}

	repo, ok := s.repos[repoName]
	if !ok {
		http.Error(w, "No such repo", 404)
		return
	}

	data, err := buildFileData(path, repo, commit)
	if err != nil {
		http.Error(w, "Error reading file", 500)
		return
	}

	script_data := &struct {
		RepoInfo config.RepoConfig `json:"repo_info"`
		Commit   string            `json:"commit"`
	}{repo, commit}

	body, err := executeTemplate(s.T.FileView, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         data.PathSegments[len(data.PathSegments)-1].Name,
		ScriptName:    "fileview",
		ScriptData:    script_data,
		IncludeHeader: false,
		Body:          template.HTML(body),
	})
}

func (s *server) parseBlameURL(r *http.Request) (string, string, error) {
	if len(s.repos) == 0 {
		return "", "", fmt.Errorf("File browsing not enabled")
	}
	repoName := r.URL.Query().Get(":repo")
	hash := r.URL.Query().Get(":hash")
	return repoName, hash, nil
}

func (s *server) ServeBlame(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	repoName, hash, err := s.parseBlameURL(r)
	if err != nil {
		http.Error(w, fmt.Sprint("404 ", err), 404)
		return
	}

	repo, ok := s.repos[repoName]
	if !ok {
		http.Error(w, "No such repo", 404)
		return
	}

	gitHistory, ok := histories[repo.Name]
	if !ok {
		http.Error(w, "Repo not configured for blame", 404)
	}

	rest := pat.Tail("/blame/:repo/:hash/", r.URL.Path)
	i := strings.LastIndex(rest, "/")
	if i == -1 {
		http.Error(w, "Not found", 404)
		return
	}
	path := rest[:i]
	if i+1 < len(rest) {
		dest := rest[i+1:]
		url, err := fileRedirect(gitHistory, repoName, hash, path, dest)
		if err != nil {
			http.Error(w, "Not found", 404)
			return
		}
		http.Redirect(w, r, url, 307)
		return
	}
	//commitHash := rest[i+1:]

	//fmt.Print(repoName, " ", hash, " ", path, "\n")

	isDiff := false // TODO: remove
	data := BlameData{}
	resolveCommit(repo, hash, &data)
	if data.CommitHash != hash {
		pat1 := "/" + hash + "/"
		pat2 := "/" + data.CommitHash + "/"
		destURL := strings.Replace(r.URL.Path, pat1, pat2, 1)
		http.Redirect(w, r, destURL, 307)
	}
	err = buildBlameData(repo, hash, gitHistory, path, isDiff, &data)
	if err != nil {
		http.Error(w, err.Error(), 404)
		return
	}
	t := s.T.BlameFile
	if isDiff {
		t = s.T.BlameDiff
	}
	err = t.Execute(w, map[string]interface{}{
		"cssTag": templates.LinkTag("stylesheet",
			"/assets/css/blame.css", s.AssetHashes),
		"repo": repo,
		"path": path,
		"commitHash": hash,
		"blame": data,
		"content": data.Content,
	})
	if err != nil {
		stdlog.Print("Cannot render template: ", err)
	}
}

func (s *server) ServeDiff(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	if len(s.repos) == 0 {
		http.Error(w, "404 Repository browsing not enabled", 404)
		return
	}
	repoName := r.URL.Query().Get(":repo")
	hash := r.URL.Query().Get(":hash")
	fmt.Print("=====", repoName, "\n")
	repo, ok := s.repos[repoName]
	if !ok {
		http.Error(w, "404 No such repository", 404)
		return
	}
	rest := pat.Tail("/diff/:repo/:hash/", r.URL.Path)
	if len(rest) > 0 {
		diffRedirect(w, r, repoName, hash, rest)
		return
	}
	data := DiffData{}
	data2 := BlameData{}
	resolveCommit(repo, hash, &data2)
	data.CommitHash = data2.CommitHash
	data.Author = data2.Author
	data.Date = data2.Date
	data.Subject = data2.Subject
	// TODO: fix this
	// if data.CommitHash != commitHash {
	// 	http.Redirect(w, r, data.CommitHash, 307)
	// }
	fmt.Print(data, "\n")

	j := strings.LastIndex(r.URL.Path, "/")
	if j < len(r.URL.Path) - 1 {
		destination := r.URL.Path[j+1:]
		fmt.Print(destination, "\n")
		// TODO: figure out redirect
		return
	}

	err := buildDiffData(repo, hash, &data)
	if err != nil {
		http.Error(w, err.Error(), 404)
		return
	}

	err = s.T.BlameDiff.Execute(w, map[string]interface{}{
		"cssTag": templates.LinkTag("stylesheet",
			"/assets/css/blame.css", s.AssetHashes),
		"repo": repo,
		"path": "NONE",
		"commitHash": hash,
		"blame": data,
	})
	if err != nil {
		stdlog.Print("Cannot render template: ", err)
	}
}

func (s *server) ServeAbout(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	body, err := executeTemplate(s.T.About, nil)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         "about",
		IncludeHeader: true,
		Body:          template.HTML(body),
	})
}

func (s *server) ServeHelp(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	// Help is now shown in the main search page when no search has been entered.
	http.Redirect(w, r, "/search", 303)
}

func (s *server) ServeHealthcheck(w http.ResponseWriter, r *http.Request) {
	// All backends must have (at some point) reported an index age for us to
	// report as healthy.
	// TODO: report as unhealthy if a backend goes down after we've spoken to
	// it.
	for _, bk := range s.bk {
		if bk.I.IndexTime.IsZero() {
			http.Error(w, fmt.Sprintf("unhealthy backend '%s' '%s'\n", bk.Id, bk.Addr), 500)
			return
		}
	}
	io.WriteString(w, "ok\n")
}

type stats struct {
	IndexAge int64 `json:"index_age"`
}

func (s *server) ServeStats(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	// For index age, report the age of the stalest backend's index.
	now := time.Now()
	maxBkAge := time.Duration(-1) * time.Second
	for _, bk := range s.bk {
		if bk.I.IndexTime.IsZero() {
			// backend didn't report index time
			continue
		}
		bkAge := now.Sub(bk.I.IndexTime)
		if bkAge > maxBkAge {
			maxBkAge = bkAge
		}
	}
	replyJSON(ctx, w, 200, &stats{
		IndexAge: int64(maxBkAge / time.Second),
	})
}

func (s *server) requestProtocol(r *http.Request) string {
	if s.config.ReverseProxy {
		if proto := r.Header.Get("X-Real-Proto"); len(proto) > 0 {
			return proto
		}
	}
	if r.TLS != nil {
		return "https"
	}
	return "http"
}

func (s *server) ServeOpensearch(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	data := &struct {
		BackendName, BaseURL string
	}{
		BaseURL: s.requestProtocol(r) + "://" + r.Host + "/",
	}

	for _, bk := range s.bk {
		if bk.I.Name != "" {
			data.BackendName = bk.I.Name
			break
		}
	}

	body, err := executeTemplate(s.T.OpenSearch, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}

	w.Header().Set("Content-Type", "application/xml")
	w.Write(body)
}

type handler func(c context.Context, w http.ResponseWriter, r *http.Request)

const RequestTimeout = 8 * time.Second

func (h handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	ctx := context.Background()
	ctx, cancel := context.WithTimeout(ctx, RequestTimeout)
	defer cancel()
	ctx = reqid.NewContext(ctx, reqid.New())
	log.Printf(ctx, "http request: remote=%q method=%q url=%q",
		r.RemoteAddr, r.Method, r.URL)
	h(ctx, w, r)
}

func (s *server) Handler(f func(c context.Context, w http.ResponseWriter, r *http.Request)) http.Handler {
	return handler(f)
}

func New(cfg *config.Config) (http.Handler, error) {
	srv := &server{
		config: cfg,
		bk:     make(map[string]*Backend),
		repos:  make(map[string]config.RepoConfig),
	}
	srv.loadTemplates()

	ctx := context.Background()

	log.Printf(ctx, "Loading blame...")
	start := time.Now()
	err := initBlame(cfg)
	if err != nil {
		log.Printf(ctx, "Error: %s", err)
		return nil, err
	}
	elapsed := time.Since(start)
	log.Printf(ctx, "Blame loaded in %s", elapsed)

	if cfg.Honeycomb.WriteKey != "" {
		log.Printf(context.Background(),
			"Enabling honeycomb dataset=%s", cfg.Honeycomb.Dataset)
		srv.honey = libhoney.NewBuilder()
		srv.honey.WriteKey = cfg.Honeycomb.WriteKey
		srv.honey.Dataset = cfg.Honeycomb.Dataset
	}

	for _, bk := range srv.config.Backends {
		be, e := NewBackend(bk.Id, bk.Addr)
		if e != nil {
			return nil, e
		}
		be.Start()
		srv.bk[be.Id] = be
		srv.bkOrder = append(srv.bkOrder, be.Id)
	}

	for _, r := range srv.config.IndexConfig.Repositories {
		srv.repos[r.Name] = r
	}

	m := pat.New()
	m.Add("GET", "/blame/:repo/:hash/", srv.Handler(srv.ServeBlame))
	m.Add("GET", "/diff/:repo/:hash/", srv.Handler(srv.ServeDiff))
	m.Add("GET", "/debug/healthcheck", http.HandlerFunc(srv.ServeHealthcheck))
	m.Add("GET", "/debug/stats", srv.Handler(srv.ServeStats))
	m.Add("GET", "/search/:backend", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/search/", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/view/:repo/", srv.Handler(srv.ServeFile))
	m.Add("GET", "/about", srv.Handler(srv.ServeAbout))
	m.Add("GET", "/help", srv.Handler(srv.ServeHelp))
	m.Add("GET", "/opensearch.xml", srv.Handler(srv.ServeOpensearch))
	m.Add("GET", "/", srv.Handler(srv.ServeSearch))

	m.Add("GET", "/api/v1/search/:backend", srv.Handler(srv.ServeAPISearch))
	m.Add("GET", "/api/v1/search/", srv.Handler(srv.ServeAPISearch))

	var h http.Handler = m

	if cfg.Reload {
		h = templates.ReloadHandler(
			path.Join(srv.config.DocRoot, "templates"),
			&srv.T,
			path.Join(srv.config.DocRoot, "hashes.txt"),
			srv.AssetHashes,
			h)
	}

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/", h)

	srv.inner = mux

	return srv, nil
}
