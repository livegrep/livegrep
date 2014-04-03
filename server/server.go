package server

import (
	"code.google.com/p/go.net/websocket"
	"github.com/bmizerany/pat"
	"github.com/nelhage/livegrep/config"
	"github.com/nelhage/livegrep/server/backend"
	"html/template"
	"net/http"
	"path"
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

func (s *server) ServeRoot(w http.ResponseWriter, r *http.Request) {
	http.Redirect(w, r, "/search", 303)
}

func (s *server) ServeSearch(w http.ResponseWriter, r *http.Request) {
	gh := make(map[string]map[string]string, len(s.bk))
	backends := make([]*backend.Backend, 0, len(s.bk))
	for _, bk := range s.bk {
		backends = append(backends, bk)
		bk.I.Lock()
		m := make(map[string]string, len(bk.I.Repos))
		gh[bk.Id] = m
		for _, r := range bk.I.Repos {
			if r.Github != "" {
				m[r.Name] = r.Github
			}
		}
		bk.I.Unlock()
	}
	ctx := &searchContext{
		GithubRepos: gh,
		Backends:    backends,
	}
	body, err := s.executeTemplate(s.t.searchPage, ctx)
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

func (s *server) ServeAbout(w http.ResponseWriter, r *http.Request) {
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

func (s *server) ServeOpensearch(w http.ResponseWriter, r *http.Request) {
	var baseURL string
	if r.TLS != nil {
		baseURL = "https://"
	} else {
		baseURL = "http://"
	}
	baseURL += r.Host + "/"
	body, err := s.executeTemplate(s.t.opensearchXML, &opensearchContext{
		BackendName: "Linux",
		BaseURL:     baseURL,
	})
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	w.Write(body)
}

func New(cfg *config.Config) (http.Handler, error) {
	srv := &server{config: cfg, bk: make(map[string]*backend.Backend)}
	srv.loadTemplates()

	for _, bk := range srv.config.Backends {
		srv.bk[bk.Id] = backend.New(&bk)
	}

	m := pat.New()
	m.Add("GET", "/", http.HandlerFunc(srv.ServeRoot))
	m.Add("GET", "/search", http.HandlerFunc(srv.ServeSearch))
	m.Add("GET", "/search/:backend", http.HandlerFunc(srv.ServeSearch))
	m.Add("GET", "/about", http.HandlerFunc(srv.ServeAbout))
	m.Add("GET", "/opensearch.xml", http.HandlerFunc(srv.ServeOpensearch))

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/socket", websocket.Handler(srv.HandleWebsocket))
	mux.Handle("/", m)

	srv.inner = &requestLogger{mux}

	return srv, nil
}
