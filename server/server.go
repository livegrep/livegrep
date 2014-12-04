package server

import (
	"html/template"
	"net/http"
	"path"
	"time"

	"code.google.com/p/go.net/context"

	"github.com/bmizerany/pat"
	"github.com/livegrep/livegrep/server/backend"
	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/livegrep/livegrep/server/reqid"
)

type server struct {
	config *config.Config
	bk     map[string]*backend.Backend
	inner  http.Handler
	t      templates
}

func (s *server) loadTemplates() {
	s.t.layout = s.readTemplates("layout.html")
	s.t.searchPage = s.readTemplates("index.html")
	s.t.aboutPage = s.readTemplates("about.html")
	s.t.opensearchXML = s.readTemplates("opensearch.xml")
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.inner.ServeHTTP(w, r)
}

func (s *server) ServeRoot(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	http.Redirect(w, r, "/search", 303)
}

func (s *server) ServeSearch(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	gh := make(map[string]map[string]string, len(s.bk))
	backends := make([]*backend.Backend, 0, len(s.bk))
	for _, bk := range s.bk {
		backends = append(backends, bk)
		bk.I.Lock()
		m := make(map[string]string, len(bk.I.Trees))
		gh[bk.Id] = m
		for _, r := range bk.I.Trees {
			if r.Github != "" {
				m[r.Name] = r.Github
			}
		}
		bk.I.Unlock()
	}
	data := &searchContext{
		GithubRepos: gh,
		Backends:    backends,
	}
	body, err := s.executeTemplate(s.t.searchPage, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:     "search",
		IncludeJS: true,
		Body:      template.HTML(body),
	})
}

func (s *server) ServeAbout(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	body, err := s.executeTemplate(s.t.aboutPage, nil)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title:     "about",
		IncludeJS: true,
		Body:      template.HTML(body),
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
	data := &opensearchContext{}
	data.BaseURL += s.requestProtocol(r) + "://" + r.Host + "/"

	for _, bk := range s.bk {
		if bk.I.Name != "" {
			data.BackendName = bk.I.Name
			break
		}
	}

	body, err := s.executeTemplate(s.t.opensearchXML, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	w.Write(body)
}

type handler func(c context.Context, w http.ResponseWriter, r *http.Request)

const RequestTimeout = 4 * time.Second

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
	srv := &server{config: cfg, bk: make(map[string]*backend.Backend)}
	srv.loadTemplates()

	for _, bk := range srv.config.Backends {
		srv.bk[bk.Id] = backend.New(&bk)
	}

	m := pat.New()
	m.Add("GET", "/", srv.Handler(srv.ServeRoot))
	m.Add("GET", "/search/", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/search/:backend", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/about", srv.Handler(srv.ServeAbout))
	m.Add("GET", "/opensearch.xml", srv.Handler(srv.ServeOpensearch))

	m.Add("GET", "/api/v1/search/", srv.Handler(srv.ServeAPISearch))
	m.Add("GET", "/api/v1/search/:backend", srv.Handler(srv.ServeAPISearch))

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/", m)

	srv.inner = mux

	return srv, nil
}
