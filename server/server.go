package server

import (
	"fmt"
	"html/template"
	"io"
	"net/http"
	"path"
	texttemplate "text/template"
	"time"

	"code.google.com/p/go.net/context"

	"github.com/bmizerany/pat"

	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/livegrep/livegrep/server/reqid"
	"github.com/livegrep/livegrep/server/templates"
)

type Templates struct {
	Layout,
	Index,
	About,
	Help *template.Template
	OpenSearch *texttemplate.Template `template:"opensearch.xml"`
}

type server struct {
	config *config.Config
	bk     map[string]*Backend
	inner  http.Handler
	T      Templates
	Layout *template.Template
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
	gh := make(map[string]map[string]string, len(s.bk))
	backends := make([]*Backend, 0, len(s.bk))
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
	data := &struct {
		GithubRepos map[string]map[string]string
		Backends    []*Backend
	}{gh, backends}

	body, err := executeTemplate(s.T.Index, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title: "search",
		Body:  template.HTML(body),
	})
}

func (s *server) ServeAbout(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	body, err := executeTemplate(s.T.About, nil)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	s.renderPage(w, &page{
		Title: "about",
		Body:  template.HTML(body),
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
		Title: "query syntax",
		Body:  template.HTML(body),
	})
}

func (s *server) ServeHealthcheck(w http.ResponseWriter, r *http.Request) {
	io.WriteString(w, "ok\n")
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

	body, err := executeTextTemplate(s.T.OpenSearch, data)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}

	w.Header().Set("Content-Type", "application/xml")
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
	srv := &server{config: cfg, bk: make(map[string]*Backend)}
	srv.loadTemplates()

	for _, bk := range srv.config.Backends {
		addr := bk.Addr
		be := &Backend{
			Id:   bk.Id,
			Dial: func() (client.Client, error) { return client.Dial("tcp", addr) },
		}
		be.Start()
		srv.bk[be.Id] = be
	}

	m := pat.New()
	m.Add("GET", "/debug/healthcheck", http.HandlerFunc(srv.ServeHealthcheck))
	m.Add("GET", "/search/:backend", srv.Handler(srv.ServeSearch))
	m.Add("GET", "/search/", srv.Handler(srv.ServeSearch))
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
