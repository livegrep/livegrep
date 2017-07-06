package server

import (
	"fmt"
	"html/template"
	"io"
	"net/http"
	"path"
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
	About,
	Help *template.Template
	OpenSearch *texttemplate.Template `template:"opensearch.xml"`
}

type server struct {
	config  *config.Config
	bk      map[string]*Backend
	bkOrder []string
	repos   map[string]config.RepoConfig
	inner   http.Handler
	T       Templates
	Layout  *template.Template

	honey *libhoney.Builder
}

func (s *server) loadTemplates() {
	if e := templates.Load(path.Join(s.config.DocRoot, "templates"), &s.T); e != nil {
		panic(fmt.Sprintf("loading templates: %v", e))
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
	for _, bkId := range s.bkOrder {
		bk := s.bk[bkId]
		backends = append(backends, bk)
		bk.I.Lock()
		m := make(map[string]string, len(bk.I.Trees))
		urls[bk.Id] = m
		for _, r := range bk.I.Trees {
			m[r.Name] = r.Url
		}
		bk.I.Unlock()
	}
	data := &struct {
		RepoUrls          map[string]map[string]string
		InternalViewRepos map[string]config.RepoConfig
		Backends          []*Backend
	}{urls, s.repos, backends}

	body, err := executeTemplate(s.T.Index, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         "search",
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

	body, err := executeTemplate(s.T.FileView, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         "file",
		IncludeHeader: false,
		Body:          template.HTML(body),
	})
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
	d := struct{ SampleRepo string }{}
	for _, bk := range s.bk {
		if len(bk.I.Trees) > 1 {
			d.SampleRepo = bk.I.Trees[0].Name
		}
	}

	body, err := executeTemplate(s.T.Help, d)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:         "query syntax",
		IncludeHeader: true,
		Body:          template.HTML(body),
	})
}

func (s *server) ServeHealthcheck(w http.ResponseWriter, r *http.Request) {
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
	m.Add("GET", "/debug/healthcheck", http.HandlerFunc(srv.ServeHealthcheck))
	m.Add("GET", "/debug/stats", srv.Handler(srv.ServeStats))
	m.Add("GET", "/search/:backend", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/search/", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/view/:repo/", srv.Handler(srv.ServeFile))
	m.Add("GET", "/about", srv.Handler(srv.ServeAbout))
	m.Add("GET", "/help", srv.Handler(srv.ServeHelp))
	m.Add("GET", "/opensearch.xml", srv.Handler(srv.ServeOpensearch))
	m.Add("GET", "/", srv.Handler(srv.ServeRoot))

	m.Add("GET", "/api/v1/search/:backend", srv.Handler(srv.ServeAPISearch))
	m.Add("GET", "/api/v1/search/", srv.Handler(srv.ServeAPISearch))

	var h http.Handler = m

	if cfg.Reload {
		h = templates.ReloadHandler(
			path.Join(srv.config.DocRoot, "templates"),
			&srv.T, h)
	}

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/", h)

	srv.inner = mux

	return srv, nil
}
