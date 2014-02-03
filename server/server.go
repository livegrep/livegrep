package server

import (
	"flag"
	"github.com/bmizerany/pat"
	"github.com/nelhage/livegrep/client"
	"html/template"
	"net/http"
	"path"
)

var (
	docRoot    *string = flag.String("docroot", "./web", "The livegrep document root (web/ directory)")
	production *bool   = flag.Bool("production", false, "Is livegrep running in production?")
)

type server struct {
	client *client.Client
	inner  http.Handler
	t      templates
}

func (s *server) loadTemplates() {
	s.t.layout = readTemplates("layout.html")
	s.t.searchPage = readTemplates("index.html")
	s.t.aboutPage = readTemplates("about.html")
	s.t.opensearchXML = readTemplates("opensearch.xml")
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.inner.ServeHTTP(w, r)
}

func (s *server) ServeRoot(w http.ResponseWriter, r *http.Request) {
	http.Redirect(w, r, "/search", 303)
}

func (s *server) ServeSearch(w http.ResponseWriter, r *http.Request) {
	ctx := &searchContext{
		GithubRepos: nil,
		Repos:       []repo{{"linux", "Linux 3.12"}},
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

func (s *server) ServeFeedback(w http.ResponseWriter, r *http.Request) {
}

func Handler(proto, addr string) (http.Handler, error) {
	srv := &server{}
	srv.loadTemplates()

	m := pat.New()
	m.Add("GET", "/", http.HandlerFunc(srv.ServeRoot))
	m.Add("GET", "/search", http.HandlerFunc(srv.ServeSearch))
	m.Add("GET", "/search/:backend", http.HandlerFunc(srv.ServeSearch))
	m.Add("GET", "/about", http.HandlerFunc(srv.ServeAbout))
	m.Add("GET", "/opensearch.xml", http.HandlerFunc(srv.ServeOpensearch))
	m.Add("POST", "/feedback", http.HandlerFunc(srv.ServeFeedback))

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(*docRoot, "htdocs"))))
	mux.Handle("/", m)

	srv.inner = mux

	return srv, nil
}
