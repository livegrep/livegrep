package server

import (
	"github.com/golang/glog"
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
	glog.Infof("request: remote=%s method=%s url=%s",
		r.RemoteAddr, r.Method, r.URL)
}
