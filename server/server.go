package server

import (
	"code.google.com/p/go.net/websocket"
	"encoding/json"
	"fmt"
	"github.com/bmizerany/pat"
	"github.com/golang/glog"
	"github.com/nelhage/livegrep/config"
	"github.com/nelhage/livegrep/server/backend"
	"html/template"
	"io"
	"net/http"
	"net/smtp"
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

type FeedbackPost struct {
	Email string `json:"email"`
	Text  string `json:"text"`
}

func (s *server) sendFeedback(r *http.Request, fb *FeedbackPost) error {
	if s.config.Feedback.MailTo != "" {
		text := fmt.Sprintf(`Codesearch feedback from %s
IP: %s
--------
%s
`, fb.Email, r.RemoteAddr, fb.Text)
		return smtp.SendMail("localhost:25", nil,
			"Codesearch <feedback@livegrep.com>",
			[]string{s.config.Feedback.MailTo},
			[]byte(text))

	} else {
		glog.Infof("feedback post=%s",
			asJSON{fb})
	}
	return nil
}

func (s *server) ServeFeedback(w http.ResponseWriter, r *http.Request) {
	body := r.FormValue("data")
	var msg FeedbackPost
	if err := json.Unmarshal([]byte(body), &msg); err != nil {
		http.Error(w, err.Error(), 400)
		return
	}
	if err := s.sendFeedback(r, &msg); err != nil {
		glog.Infof("while sending feedback: %s", err.Error())
		http.Error(w, err.Error(), 500)
	} else {
		io.WriteString(w, "OK")
	}

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
	m.Add("POST", "/feedback", http.HandlerFunc(srv.ServeFeedback))

	mux := http.NewServeMux()
	mux.Handle("/assets/", http.FileServer(http.Dir(path.Join(cfg.DocRoot, "htdocs"))))
	mux.Handle("/socket", websocket.Handler(srv.HandleWebsocket))
	mux.Handle("/", m)

	srv.inner = &requestLogger{mux}

	return srv, nil
}
