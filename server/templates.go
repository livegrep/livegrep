package server

import (
	"bytes"
	"io"
	"log"

	"html/template"

	"github.com/livegrep/livegrep/server/config"
)

type page struct {
	Title         string
	ScriptName    string
	ScriptData    interface{}
	IncludeHeader bool
	Body          template.HTML
	Config        *config.Config
}

type Template interface {
	Execute(wr io.Writer, data interface{}) error
}

func executeTemplate(t Template, context interface{}) ([]byte, error) {
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
