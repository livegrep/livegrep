package server

import (
	"fmt"
	"time"

	"code.google.com/p/go.net/websocket"
	"github.com/golang/glog"
	"github.com/nelhage/livegrep/client"
	"github.com/nelhage/livegrep/jsonframe"
	"github.com/nelhage/livegrep/server/backend"
)

type searchConnection struct {
	srv      *server
	ws       *websocket.Conn
	backend  *backend.Backend
	client   client.Client
	errors   chan error
	incoming chan jsonframe.Op
	outgoing chan jsonframe.Op
	shutdown bool
	q        struct {
		last *OpQuery
		// The time we dispatched 'last'
		t    time.Time
		next *OpQuery
	}
}

func (s *searchConnection) recvLoop() {
	var op jsonframe.Op
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
		Line:     q.Line,
		File:     q.File,
		Repo:     q.Repo,
		FoldCase: q.FoldCase,
	}
}

func (s *searchConnection) handle() {
	s.incoming = make(chan jsonframe.Op, 1)
	s.outgoing = make(chan jsonframe.Op, 1)
	s.errors = make(chan error, 1)

	go s.recvLoop()
	go s.sendLoop()
	defer close(s.outgoing)

	var search client.Search
	var results <-chan *client.Result
	var err error

	var clients <-chan client.Client

SearchLoop:
	for {
		select {
		case op, ok := <-s.incoming:
			if !ok {
				break
			}
			switch t := op.(type) {
			case *OpQuery:
				s.q.next = t
			default:
				s.outgoing <- &OpError{fmt.Sprintf("Invalid opcode %s", op.Opcode())}
				break
			}

		case e := <-s.errors:
			glog.Infof("error reading from client remote=%s error=%s\n",
				s.ws.Request().RemoteAddr,
				e.Error())
			break SearchLoop
		case res, ok := <-results:
			if ok {
				s.outgoing <- &OpResult{s.q.last.Id, res}
			} else {
				st, err := search.Close()
				s.backend.CheckIn(s.client)
				s.client = nil
				s.backend = nil
				if err == nil {
					duration := time.Since(s.q.t)
					glog.Infof("search done remote=%s id=%d query=%s millis=%d",
						s.ws.Request().RemoteAddr,
						s.q.last.Id,
						asJSON{query(s.q.last)},
						int64(duration/time.Millisecond))
					s.outgoing <- &OpSearchDone{s.q.last.Id, int64(duration / time.Millisecond), st}
				} else if _, ok := err.(client.QueryError); ok {
					s.outgoing <- &OpQueryError{s.q.last.Id, err.Error()}
				} else {
					glog.Infof("internal error doing search remote=%s id=%d error=%s",
						s.ws.Request().RemoteAddr,
						s.q.last.Id, asJSON{err.Error()})
					if s.q.next == nil {
						// retry the search
						s.q.next = s.q.last
						s.q.last = nil
					}
				}
				results = nil
				search = nil
			}
		case s.client = <-clients:
			clients = nil
			q := query(s.q.next)
			glog.Infof("dispatching remote=%s id=%d query=%s",
				s.ws.Request().RemoteAddr,
				s.q.next.Id,
				asJSON{q})
			search, err = s.client.Query(q)
			s.q.t = time.Now()
			if err != nil {
				s.outgoing <- &OpQueryError{s.q.next.Id, err.Error()}
			} else {
				if search == nil {
					panic("nil search and nil error?")
				}
				s.q.last = s.q.next
				results = search.Results()
			}
			s.q.next = nil
		}
		if s.q.next != nil && results == nil {
			if !s.shouldDispatch(s.q.next) {
				s.q.next = nil
				continue
			}
			if err = s.connectBackend(s.q.next.Backend); err != nil {
				s.outgoing <- &OpQueryError{s.q.next.Id, err.Error()}
				s.q.next = nil
				continue
			}
			clients = s.backend.Clients
		}
	}

	s.shutdown = true
}

func (s *searchConnection) shouldDispatch(q *OpQuery) bool {
	if s.q.last == nil {
		return true
	}
	if s.q.last.Backend != q.Backend ||
		s.q.last.Line != q.Line ||
		s.q.last.File != q.File ||
		s.q.last.Repo != q.Repo ||
		s.q.last.FoldCase != q.FoldCase {
		return true
	}
	return false
}

func (s *searchConnection) connectBackend(backend string) error {
	var ok bool
	s.backend, ok = s.srv.bk[backend]
	if !ok {
		return fmt.Errorf("no such backend: %s", backend)
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
