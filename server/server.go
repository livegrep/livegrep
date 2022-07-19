package server

import (
	"bytes"
	"fmt"
	"html/template"
	"io"
	"net/http"
	"path"
	"regexp"
	"sort"
	"strconv"
	texttemplate "text/template"
	"time"

	"golang.org/x/net/context"

	"github.com/bmizerany/pat"
	"github.com/honeycombio/libhoney-go"

	"github.com/livegrep/livegrep/server/langserver"

	"path/filepath"
	"strings"

	"errors"
	"net/url"

	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/livegrep/livegrep/server/reqid"
	"github.com/livegrep/livegrep/server/templates"
)

var serveUrlParseError = fmt.Errorf("failed to parse repo and path from URL")

type page struct {
	Title         string
	ScriptName    string
	ScriptData    interface{}
	IncludeHeader bool
	Data          interface{}
	Config        *config.Config
	AssetHashes   map[string]string
	Nonce         template.HTMLAttr // either `` or ` nonce="..."`
}

type server struct {
	config      *config.Config
	bk          map[string]*Backend
	bkOrder     []string
	repos       map[string]config.RepoConfig
	langsrv     map[string]langserver.Client
	inner       http.Handler
	Templates   map[string]*template.Template
	OpenSearch  *texttemplate.Template
	AssetHashes map[string]string
	Layout      *template.Template

	honey *libhoney.Builder

	serveFilePathRegex *regexp.Regexp
}

const (
	repoNameParamName  = "repo_name"
	lineParamName      = "line"
	filePathParamName  = "file_path"
	characterParamName = "character"
)

func (s *server) loadTemplates() {
	s.Templates = make(map[string]*template.Template)
	err := templates.LoadTemplates(s.config.DocRoot, s.Templates)
	if err != nil {
		panic(fmt.Sprintf("loading templates: %v", err))
	}

	p := s.config.DocRoot + "/templates/opensearch.xml"
	s.OpenSearch = texttemplate.Must(texttemplate.ParseFiles(p))

	s.AssetHashes = make(map[string]string)
	err = templates.LoadAssetHashes(
		path.Join(s.config.DocRoot, "hashes.txt"),
		s.AssetHashes)
	if err != nil {
		panic(fmt.Sprintf("loading templates: %v", err))
	}
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.inner.ServeHTTP(w, r)
}

func (s *server) ServeRoot(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	http.Redirect(w, r, "/search", 303)
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

	script_data := &struct {
		RepoUrls           map[string]map[string]string `json:"repo_urls"`
		InternalViewRepos  map[string]config.RepoConfig `json:"internal_view_repos"`
		DefaultSearchRepos []string                     `json:"default_search_repos"`
		LinkConfigs        []config.LinkConfig          `json:"link_configs"`
	}{urls, s.repos, s.config.DefaultSearchRepos, s.config.LinkConfigs}

	s.renderPage(ctx, w, r, "index.html", &page{
		Title:         "code search",
		ScriptName:    "codesearch",
		ScriptData:    script_data,
		IncludeHeader: true,
		Data: struct {
			Backends   []*Backend
			SampleRepo string
		}{
			Backends:   backends,
			SampleRepo: sampleRepo,
		},
	})
}

func (s *server) ServeFile(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	repoName, path, err := getRepoPathFromURL(s.serveFilePathRegex, r.URL.Path)
	if err != nil {
		http.Error(w, err.Error(), 400)
		return
	}

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

	langServer := langserver.ForFile(&repo, path)
	script_data := &struct {
		RepoInfo      config.RepoConfig `json:"repo_info"`
		Commit        string            `json:"commit"`
		FilePath      string            `json:"file_path"`
		HasLangServer bool              `json:"has_lang_server"`
	}{repo, commit, path, langServer != nil}

	s.renderPage(ctx, w, r, "fileview.html", &page{
		Title:         data.PathSegments[len(data.PathSegments)-1].Name,
		ScriptName:    "fileview",
		ScriptData:    script_data,
		IncludeHeader: false,
		Data:          data,
	})
}

func (s *server) ServeAbout(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	s.renderPage(ctx, w, r, "about.html", &page{
		Title:         "about",
		IncludeHeader: true,
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

	templateName := "opensearch.xml"
	w.Header().Set("Content-Type", "application/xml")
	err := s.OpenSearch.ExecuteTemplate(w, templateName, data)
	if err != nil {
		log.Printf(ctx, "Error rendering %s: %s", templateName, err)
		return
	}
}

func (s *server) renderPage(ctx context.Context, w io.Writer, r *http.Request, templateName string, pageData *page) {
	t, ok := s.Templates[templateName]
	if !ok {
		log.Printf(ctx, "Error: no template named %v", templateName)
		return
	}

	pageData.Config = s.config
	pageData.AssetHashes = s.AssetHashes

	nonce := "" // custom nonce computation can go here

	if nonce != "" {
		pageData.Nonce = template.HTMLAttr(fmt.Sprintf(` nonce="%s"`, nonce))
	}

	err := t.ExecuteTemplate(w, templateName, pageData)
	if err != nil {
		log.Printf(ctx, "Error rendering %v: %s", templateName, err)
		return
	}
}

type reloadHandler struct {
	srv   *server
	inner http.Handler
}

func (h *reloadHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	h.srv.loadTemplates()
	h.inner.ServeHTTP(w, r)
}

type DefinitionResponse struct {
	URL string `json:"url"`
}

func (s *server) ServeDefinition(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	docPositionParams, repo, err := s.parseDocPositionParams(r.URL.Query())
	if err != nil {
		writeError(ctx, w, 400, "bad_arguments", err.Error())
		return
	}

	l := langserver.ForFile(repo, docPositionParams.TextDocument.URI)
	langServer := s.langsrv[l.Address]
	if langServer == nil {
		// no language server
		writeError(ctx, w, 404, "not_found", err.Error())
		return
	}

	locations, err := langServer.Definition(ctx, docPositionParams)
	if err != nil {
		writeError(ctx, w, 500, "lsp_error", err.Error())
		return
	}

	retUrl := ""

	if len(locations) > 0 {
		location := locations[0]
		targetPath := strings.TrimPrefix(location.URI, "file://")
		// Add 1 because URL is 1-indexed and language server is 0-indexed.
		lineNum := location.Range.Start.Line + 1
		if !strings.HasPrefix(targetPath, repo.Path) {
			writeError(ctx, w, 400, "out_of_repo", "locations outside repo not supported")
			return
		}

		relPath, err := filepath.Rel(repo.Path, targetPath)
		if err != nil {
			writeError(ctx, w, 500, "invalid_path", err.Error())
			return
		}

		retUrl = fmt.Sprintf("/view/%s/%s#L%d", repo.Name, relPath, lineNum)
	} else {
		writeError(ctx, w, 400, "unresolved", "could not resolve an identifier")
		return
	}

	replyJSON(ctx, w, 200, &DefinitionResponse{
		URL: retUrl,
	})
}

func (s *server) ServeHover(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	docPositionParams, repo, err := s.parseDocPositionParams(r.URL.Query())
	if err != nil {
		writeError(ctx, w, 400, "bad_arguments", err.Error())
		return
	}

	l := langserver.ForFile(repo, docPositionParams.TextDocument.URI)
	langServer := s.langsrv[l.Address]
	if langServer == nil {
		// no language server
		writeError(ctx, w, 404, "not_found", err.Error())
		return
	}

	result, err := langServer.Hover(ctx, docPositionParams)
	if err != nil {
		writeError(ctx, w, 500, "lsp_error", err.Error())
		return
	}

	replyJSON(ctx, w, 200, result)
}

func findRepo(repos []config.RepoConfig, repoName string) (*config.RepoConfig, error) {
	for _, repo := range repos {
		if repo.Name == repoName {
			return &repo, nil
		}
	}
	return nil, fmt.Errorf("repo with name %s not found", repoName)
}

func (s *server) parseDocPositionParams(params url.Values) (*langserver.TextDocumentPositionParams, *config.RepoConfig, error) {
	lineParam, characterParam, repo, fpath :=
		params.Get(lineParamName),
		params.Get(characterParamName),
		params.Get(repoNameParamName),
		params.Get(filePathParamName)

	if lineParam == "" || characterParam == "" || repo == "" || fpath == "" {
		return nil, nil, errors.New("line, character, repo, and filepath need to be set")
	}

	line, err := strconv.Atoi(lineParam)
	if err != nil {
		return nil, nil, errors.New("line param needs to be a number, got " + lineParam)
	}

	character, err := strconv.Atoi(characterParam)
	if err != nil {
		return nil, nil, errors.New("character param needs to be a number, got " + characterParam)
	}

	repoConfig, err := findRepo(s.config.IndexConfig.Repositories, repo)
	if err != nil {
		return nil, nil, err
	}

	positionParams := &langserver.TextDocumentPositionParams{
		TextDocument: langserver.TextDocumentIdentifier{
			URI: buildURI(repoConfig.Path, fpath),
		},
		Position: langserver.Position{
			Line:      line,
			Character: character,
		},
	}

	return positionParams, repoConfig, nil
}

func buildURI(repoPath string, relativeFilePath string) string {
	return "file://" + repoPath + "/" + relativeFilePath
}

type handler func(c context.Context, w http.ResponseWriter, r *http.Request)

const RequestTimeout = 30 * time.Second

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
		config:  cfg,
		bk:      make(map[string]*Backend),
		repos:   make(map[string]config.RepoConfig),
		langsrv: make(map[string]langserver.Client),
	}
	srv.loadTemplates()

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

	var repoNames []string
	for _, r := range srv.config.IndexConfig.Repositories {
		srv.repos[r.Name] = r
		repoNames = append(repoNames, r.Name)
	}

	serveFilePathRegex, err := buildRepoRegex(repoNames)
	if err != nil {
		return nil, err
	}
	srv.serveFilePathRegex = serveFilePathRegex

	m := pat.New()
	m.Add("GET", "/debug/healthcheck", http.HandlerFunc(srv.ServeHealthcheck))
	m.Add("GET", "/debug/stats", srv.Handler(srv.ServeStats))
	m.Add("GET", "/search/:backend", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/search/", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/view/", srv.Handler(srv.ServeFile))
	m.Add("GET", "/about", srv.Handler(srv.ServeAbout))
	m.Add("GET", "/help", srv.Handler(srv.ServeHelp))
	m.Add("GET", "/opensearch.xml", srv.Handler(srv.ServeOpensearch))
	m.Add("GET", "/", srv.Handler(srv.ServeRoot))

	m.Add("GET", "/api/v1/search/:backend", srv.Handler(srv.ServeAPISearch))
	m.Add("GET", "/api/v1/search/", srv.Handler(srv.ServeAPISearch))
	m.Add("GET", "/api/v1/lsp/definition", srv.Handler(srv.ServeDefinition))
	m.Add("GET", "/api/v1/lsp/hover", srv.Handler(srv.ServeHover))

	var h http.Handler = m

	if cfg.Reload {
		h = &reloadHandler{srv, h}
	}

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/", h)

	srv.inner = mux

	ctx := context.Background()

	for _, r := range srv.config.IndexConfig.Repositories {
		for _, langServer := range r.LangServers {

			client, err := langserver.NewClient(langServer.Address, &langserver.InitializeParams{
				ProcessId:        nil,
				OriginalRootPath: r.Path,
				RootUri:          "file://" + r.Path,
				Capabilities:     langserver.ClientCapabilities{},
			})

			// a failing language server isn't fatal. We'd prefer to log these metrics.
			if err != nil {
				log.Printf(ctx, "Failed to create LSP client: %s@%s: %s", r.Name, langServer.Address, err.Error())
				continue
			}

			srv.langsrv[langServer.Address] = client
		}

		srv.repos[r.Name] = r
		repoNames = append(repoNames, r.Name)
	}

	return srv, nil
}

func buildRepoRegex(repoNames []string) (*regexp.Regexp, error) {
	// Sort in descending order of length so most specific match is selected by regex engine
	sort.Slice(repoNames, func(i, j int) bool {
		return len(repoNames[i]) >= len(repoNames[j])
	})

	// Build regex of form "(repo1|repo2)/(path)"
	var buf bytes.Buffer
	for i, repoName := range repoNames {
		buf.WriteString(regexp.QuoteMeta(repoName))
		if i < len(repoNames)-1 {
			buf.WriteString("|")
		}
	}
	repoRegexAlt := buf.String()
	repoFileRegex, err := regexp.Compile(fmt.Sprintf("(%s)/(.*)", repoRegexAlt))
	if err != nil {
		return nil, fmt.Errorf("failed to create regular expression for URL parsing")
	}

	return repoFileRegex, nil
}

func getRepoPathFromURL(repoRegex *regexp.Regexp, url string) (repo string, path string, err error) {
	matches := repoRegex.FindStringSubmatch(pat.Tail("/view/", url))
	if len(matches) == 0 {
		return "", "", serveUrlParseError
	}

	return matches[1], matches[2], nil
}
