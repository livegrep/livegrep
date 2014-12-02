package server

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"

	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/server/api"
	"github.com/livegrep/livegrep/server/backend"
)

func replyJSON(w http.ResponseWriter, status int, obj interface{}) {
	w.WriteHeader(status)
	enc := json.NewEncoder(w)
	if err := enc.Encode(obj); err != nil {
		log.Printf("writing http response, data=%s err=%s",
			asJSON{obj},
			err.Error())
	}
}

func writeError(w http.ResponseWriter, status int, code, message string) {
	replyJSON(w, status, &api.ReplyError{Err: api.InnerError{Code: code, Message: message}})
}

func writeQueryError(w http.ResponseWriter, err error) {
	if qe, ok := err.(client.QueryError); ok {
		writeError(w, 400, "query_error", qe.Err)
	} else {
		writeError(w, 500, "internal_error",
			fmt.Sprintf("Talking to backend: %s", err.Error()))
	}
	return
}

func parseQuery(r *http.Request) client.Query {
	params := r.URL.Query()
	return client.Query{
		Line:     params.Get("line"),
		File:     params.Get("file"),
		Repo:     params.Get("repo"),
		FoldCase: params.Get("fold_case") != "",
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
		writeQueryError(w, err)
		return
	}

	reply := &api.ReplySearch{}

	for r := range search.Results() {
		reply.Results = append(reply.Results, r)
	}

	reply.Info, err = search.Close()
	if err != nil {
		writeQueryError(w, err)
		return
	}

	replyJSON(w, 200, reply)
}
