package server

import (
	"bytes"
	"io"
	"log"

	"html/template"

	"github.com/livegrep/livegrep/server/config"
)

type page struct {
	IncludeJS bool
	Title     string
	Body      template.HTML
	Config    *config.Config
}

func executeTemplate(t *template.Template, context interface{}) ([]byte, error) {
	var buf bytes.Buffer
	if err := t.Execute(&buf, context); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func (s *server) renderPage(w io.Writer, p *page) {
	p.Config = s.config
	if e := s.T.Layout.Execute(w, p); e != nil {
		log.Printf("Error rendering page=%q error=%q",
			p.Title, e.Error())
	}
}
