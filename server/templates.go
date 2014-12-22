package server

import (
	"bytes"
	"html/template"
	"io"
	"log"
	"path"

	"github.com/livegrep/livegrep/server/config"
)

type templates struct {
	layout,
	searchPage,
	aboutPage,
	opensearchXML *template.Template
}

type page struct {
	IncludeJS bool
	Title     string
	Body      template.HTML
	Config    *config.Config
}

type opensearchContext struct {
	BackendName string
	BaseURL     string
}

type searchContext struct {
	GithubRepos interface{}
	Backends    []*Backend
}

func (s *server) readTemplates(files ...string) *template.Template {
	filenames := make([]string, 0, len(files))
	for _, f := range files {
		filenames = append(filenames, path.Join(s.config.DocRoot, "templates", f))
	}
	return template.Must(template.ParseFiles(filenames...))
}

func (s *server) executeTemplate(t *template.Template, context interface{}) ([]byte, error) {
	var buf bytes.Buffer
	if err := t.Execute(&buf, context); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (s *server) renderPage(w io.Writer, p *page) {
	p.Config = s.config
	if e := s.t.layout.Execute(w, p); e != nil {
		log.Printf("Error rendering page=%q error=%q",
			p.Title, e.Error())
	}
}
