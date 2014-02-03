package server

import (
	"bytes"
	"html/template"
	"io"
	"path"
)

type templates struct {
	layout,
	searchPage,
	aboutPage,
	opensearchXML *template.Template
}

type page struct {
	IncludeJS  bool
	Production bool
	Title      string
	Body       template.HTML
}

type opensearchContext struct {
	BackendName string
	BaseURL     string
}

type repo struct {
	Name   string
	Pretty string
}

type searchContext struct {
	GithubRepos interface{}
	Repos       []repo
}

func readTemplates(files ...string) *template.Template {
	filenames := make([]string, 0, len(files))
	for _, f := range files {
		filenames = append(filenames, path.Join(*docRoot, "templates", f))
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
	// p.production = true
	s.t.layout.Execute(w, p)

}
