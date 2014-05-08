package server

import (
	"encoding/json"
	"fmt"
	"github.com/golang/glog"
	"github.com/nelhage/livegrep/client"
	"github.com/nelhage/livegrep/server/api"
	"github.com/nelhage/livegrep/server/backend"
	"net/http"
)

func replyJSON(w http.ResponseWriter, status int, obj interface{}) {
	w.WriteHeader(status)
	enc := json.NewEncoder(w)
	if err := enc.Encode(obj); err != nil {
		glog.Warningf("writing http response, data=%s err=%s",
			asJSON{obj},
			err.Error())
	}
}

func writeError(w http.ResponseWriter, status int, code, message string) {
	replyJSON(w, status, &api.ReplyError{Err: api.InnerError{Code: code, Message: message}})
}

func parseQuery(r *http.Request) client.Query {
	params := r.URL.Query()
	return client.Query{
		Line: params.Get("line"),
		File: params.Get("file"),
		Repo: params.Get("repo"),
	}
}

func (s *server) ServeAPISearch(w http.ResponseWriter, r *http.Request) {
	backendName := r.URL.Query().Get(":backend")
	var backend *backend.Backend
	if backendName != "" {
		backend = s.bk[backendName]
		if backend == nil {
			writeError(w, 400, "bad_backend", fmt.Sprintf("Unknown backend: %s", backendName))
			return
		}
	} else {
		for _, backend = range s.bk {
			break
		}
	}

	q := parseQuery(r)

	if q.Line == "" {
		writeError(w, 400, "bad_query", "You must specify a 'line' regex.")
		return
	}

	cl := <-backend.Clients
	defer backend.CheckIn(cl)

	search, err := cl.Query(&q)
	if err != nil {
		writeError(w, 500, "internal_error",
			fmt.Sprintf("Talking to backend: %s", err.Error()))
		return
	}

	reply := &api.ReplySearch{}

	for r := range search.Results() {
		reply.Results = append(reply.Results, r)
	}

	reply.Info, err = search.Close()
	if err != nil {
		writeError(w, 500, "internal_error",
			fmt.Sprintf("Talking to backend: %s", err.Error()))
		return
	}

	replyJSON(w, 200, reply)
}
