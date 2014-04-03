package middleware

import (
	"net/http"
)

type reverseProxyHandler struct {
	inner http.Handler
}

func (h *reverseProxyHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if ip := r.Header.Get("X-Real-Ip"); len(ip) > 0 {
		r.RemoteAddr = ip
	}
	if host := r.Header.Get("X-Forwarded-Host"); len(host) > 0 {
		r.Host = host
	}
	h.inner.ServeHTTP(w, r)
}

func UnwrapProxyHeaders(h http.Handler) http.Handler {
	return &reverseProxyHandler{h}
}
