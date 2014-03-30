package middleware

import (
	"net/http"
)

type realIPHandler struct {
	inner http.Handler
}

func (h *realIPHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if ip, ok := r.Header["X-Real-Ip"]; ok && len(ip) > 0 {
		r.RemoteAddr = ip[0]
	}
	h.inner.ServeHTTP(w, r)
}

func UnwrapRealIP(h http.Handler) http.Handler {
	return &realIPHandler{h}
}
