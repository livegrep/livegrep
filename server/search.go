package server

import (
	"code.google.com/p/go.net/websocket"
	"fmt"
	"github.com/golang/glog"
	"github.com/nelhage/livegrep/client"
	"time"
)

type searchConnection struct {
	srv        *server
	ws         *websocket.Conn
	backend    string
	client     client.Client
	errors     chan error
	incoming   chan Op
	outgoing   chan Op
	shutdown   bool
	lastQuery  *OpQuery
	dispatched time.Time
}

func (s *searchConnection) recvLoop() {
	var op Op
	for {
		if err := OpCodec.Receive(s.ws, &op); err != nil {
			glog.V(1).Infof("Error in receive: %s\n", err.Error())
			if _, ok := err.(*ProtocolError); ok {
				// TODO: is this a good idea?
				// s.outgoing <- &OpError{err.Error()}
				continue
			} else {
				s.errors <- err
				break
			}
		}
		glog.V(2).Infof("Incoming: %s", asJSON{op})
		s.incoming <- op
		if s.shutdown {
			break
		}
	}
	close(s.incoming)
}

func (s *searchConnection) sendLoop() {
	for op := range s.outgoing {
		glog.V(2).Infof("Outgoing: %s", asJSON{op})
		OpCodec.Send(s.ws, op)
	}
}

func query(q *OpQuery) *client.Query {
	return &client.Query{
		Line: q.Line,
		File: q.File,
		Repo: q.Repo,
	}
}

func (s *searchConnection) handle() {
	s.incoming = make(chan Op, 1)
	s.outgoing = make(chan Op, 1)
	s.errors = make(chan error, 1)

	go s.recvLoop()
	go s.sendLoop()
	defer close(s.outgoing)

	var nextQuery *OpQuery

	var search client.Search
	var results <-chan *client.Result
	var err error

SearchLoop:
	for {
		select {
		case op, ok := <-s.incoming:
			if !ok {
				break
			}
			switch t := op.(type) {
			case *OpQuery:
				nextQuery = t
			default:
				s.outgoing <- &OpError{fmt.Sprintf("Invalid opcode %s", op.Opcode())}
				break
			}

		case e := <-s.errors:
			glog.Infof("error reading from client: %s\n", e.Error())
			break SearchLoop
		case res, ok := <-results:
			if ok {
				s.outgoing <- &OpResult{s.lastQuery.Id, res}
			} else {
				st, err := search.Close()
				if err == nil {
					duration := time.Since(s.dispatched)
					s.outgoing <- &OpSearchDone{s.lastQuery.Id, int64(duration / time.Millisecond), st}
				} else {
					s.outgoing <- &OpQueryError{s.lastQuery.Id, err.Error()}
				}
				results = nil
				search = nil
			}
		}
		if nextQuery != nil && results == nil {
			if !s.shouldDispatch(nextQuery) {
				nextQuery = nil
				continue
			}
			if err := s.connectBackend(nextQuery.Backend); err != nil {
				s.outgoing <- &OpQueryError{nextQuery.Id, err.Error()}
				nextQuery = nil
				continue
			}
			q := query(nextQuery)
			glog.Infof("[%s] dispatching: %s", s.ws.Request().RemoteAddr, asJSON{q})
			search, err = s.client.Query(q)
			s.dispatched = time.Now()
			if err != nil {
				s.outgoing <- &OpQueryError{nextQuery.Id, err.Error()}
			} else {
				if search == nil {
					panic("nil search and nil error?")
				}
				s.lastQuery = nextQuery
				results = search.Results()
			}
			nextQuery = nil
		}
	}

	s.shutdown = true
}

func (s *searchConnection) shouldDispatch(q *OpQuery) bool {
	if s.lastQuery == nil {
		return true
	}
	if s.lastQuery.Backend != q.Backend ||
		s.lastQuery.Line != q.Line ||
		s.lastQuery.File != q.File ||
		s.lastQuery.Repo != q.Repo {
		return true
	}
	return false
}

func (s *searchConnection) connectBackend(backend string) error {
	if s.client == nil || s.backend != backend {
		if s.client != nil {
			s.client.Close()
		}
		s.backend = backend
		addr := ""
		for _, bk := range s.srv.config.Backends {
			if bk.Id == backend {
				addr = bk.Addr
				break
			}
		}
		if addr == "" {
			return fmt.Errorf("No such backend: %s", backend)
		}
		s.client = client.ClientWithRetry(func() (client.Client, error) {
			return client.Dial("tcp", addr)
		})
	}
	return nil
}

func (s *server) HandleWebsocket(ws *websocket.Conn) {
	c := &searchConnection{
		srv: s,
		ws:  ws,
	}
	c.handle()
}
