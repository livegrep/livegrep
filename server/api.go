package server

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"

	"code.google.com/p/go.net/context"

	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/server/api"
	"github.com/livegrep/livegrep/server/backend"
	"github.com/livegrep/livegrep/server/log"
)

func replyJSON(ctx context.Context, w http.ResponseWriter, status int, obj interface{}) {
	w.WriteHeader(status)
	enc := json.NewEncoder(w)
	if err := enc.Encode(obj); err != nil {
		log.Printf(ctx, "writing http response, data=%s err=%s",
			asJSON{obj},
			err.Error())
	}
}

func writeError(ctx context.Context, w http.ResponseWriter, status int, code, message string) {
	log.Printf(ctx, "error status=%d code=%s message=%s",
		status, code, message)
	replyJSON(ctx, w, status, &api.ReplyError{Err: api.InnerError{Code: code, Message: message}})
}

func writeQueryError(ctx context.Context, w http.ResponseWriter, err error) {
	if qe, ok := err.(client.QueryError); ok {
		writeError(ctx, w, 400, "query_error", qe.Err)
	} else {
		writeError(ctx, w, 500, "internal_error",
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

const MaxRetries = 8

var (
	ErrTimedOut = errors.New("timed out talking to backend")
)

func (s *server) doSearch(ctx context.Context, backend *backend.Backend, q *client.Query) (*api.ReplySearch, error) {
	var cl client.Client
	var search client.Search
	var err error

	select {
	case cl = <-backend.Clients:
	case <-ctx.Done():
		return nil, ErrTimedOut
	}
	defer backend.CheckIn(cl)

	search, err = cl.Query(q)
	if err != nil {
		log.Printf(ctx, "error talking to backend err=%s", err)
		return nil, err
	}

	reply := &api.ReplySearch{Results: make([]*client.Result, 0)}

	for r := range search.Results() {
		reply.Results = append(reply.Results, r)
	}

	reply.Info, err = search.Close()
	if err != nil {
		return nil, err
	}
	return reply, nil
}

func (s *server) ServeAPISearch(ctx context.Context, w http.ResponseWriter, r *http.Request) {
	backendName := r.URL.Query().Get(":backend")
	var backend *backend.Backend
	if backendName != "" {
		backend = s.bk[backendName]
		if backend == nil {
			writeError(ctx, w, 400, "bad_backend",
				fmt.Sprintf("Unknown backend: %s", backendName))
			return
		}
	} else {
		for _, backend = range s.bk {
			break
		}
	}

	q := parseQuery(r)

	if q.Line == "" {
		writeError(ctx, w, 400, "bad_query",
			"You must specify a 'line' regex.")
		return
	}

	var reply *api.ReplySearch
	var err error

	for tries := 0; tries < MaxRetries; tries++ {
		reply, err = s.doSearch(ctx, backend, &q)
		if err == nil {
			break
		}
		if err == ErrTimedOut {
			break
		}
		if _, ok := err.(*client.QueryError); ok {
			break
		}
		log.Printf(ctx, "error querying try=%d err=%s", tries, err)
	}

	if err != nil {
		log.Printf(ctx, "error in search err=%s", err)
		writeQueryError(ctx, w, err)
		return
	}

	log.Printf(ctx,
		"responding success results=%d why=%s",
		len(reply.Results),
		reply.Info.ExitReason)

	replyJSON(ctx, w, 200, reply)
}
