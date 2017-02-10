package middleware

import (
	"net/http"
	"strings"
)

type reverseProxyHandler struct {
	inner http.Handler
}

func (h *reverseProxyHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if ip := r.Header.Get("X-Forwarded-For"); len(ip) > 0 {
		r.RemoteAddr = strings.SplitN(ip, ",", 2)[0]
	}
	if proto := r.Header.Get("X-Forwarded-Proto"); proto == "http" {
		u := *r.URL
		u.Scheme = "https"
		u.Host = r.Host
		w.Header().Add("Location", u.String())
		w.WriteHeader(http.StatusMovedPermanently)
		return
	}
	h.inner.ServeHTTP(w, r)
}

func UnwrapProxyHeaders(h http.Handler) http.Handler {
	return &reverseProxyHandler{h}
}
