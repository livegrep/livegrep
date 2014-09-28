package server

import (
	"log"
	"net/http"
)

type requestLogger struct {
	inner http.Handler
}

func (s *requestLogger) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.logRequest(r)
	s.inner.ServeHTTP(w, r)
}

func (s *requestLogger) logRequest(r *http.Request) {
	log.Printf("request: remote=%q method=%q url=%q",
		r.RemoteAddr, r.Method, r.URL)
}
