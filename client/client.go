package client

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"

	"github.com/livegrep/livegrep/jsonframe"
)

var ops jsonframe.Marshaler

func init() {
	ops.Register(new(Result))
	ops.Register(new(ReplyError))
	ops.Register(new(ServerInfo))
	ops.Register(new(Stats))
	ops.Register(new(Query))
}

type client struct {
	conn    io.ReadWriteCloser
	queries chan request
	errors  chan error
	error   error
	ready   chan *ServerInfo
	info    *ServerInfo
}

type request struct {
	request  jsonframe.Op
	response chan jsonframe.Op
	errors   chan error
}

type search struct {
	q *Query
	r *request

	results chan *Result
	stats   *Stats
	err     error
}

func Dial(network, address string) (Client, error) {
	conn, err := net.Dial(network, address)
	if err != nil {
		return nil, err
	}

	return New(conn)
}

func New(conn io.ReadWriteCloser) (Client, error) {
	var err error

	cl := &client{
		conn:    conn,
		queries: make(chan request),
		errors:  make(chan error, 1),
		ready:   make(chan *ServerInfo),
	}

	go cl.loop()

	select {
	case cl.info = <-cl.ready:
	case err = <-cl.errors:
		close(cl.queries)
		return nil, err
	}

	return cl, nil
}

func (c *client) Info() *ServerInfo {
	return c.info
}

func (c *client) Err() error {
	if c.error != nil {
		return c.error
	}
	select {
	case c.error = <-c.errors:
	default:
	}
	return c.error
}

func (c *client) Query(q *Query) (Search, error) {
	s := &search{q: q,
		r:       &request{q, make(chan jsonframe.Op), make(chan error, 1)},
		results: make(chan *Result),
	}
	select {
	case e, ok := <-c.errors:
		if !ok {
			e = errors.New("use of a closed Client")
		}
		c.error = e
		return nil, e
	case c.queries <- *s.r:
		go s.loop()
		return s, nil
	}
}

func (c *client) Close() {
	close(c.queries)
}

func (c *client) loop() {
	defer c.conn.Close()
	defer close(c.errors)
	encoder := json.NewEncoder(c.conn)
	decoder := json.NewDecoder(c.conn)

	for {
		op, err := ops.Decode(decoder)
		if err != nil {
			c.errors <- err
			return
		}
		if info, ok := op.(*ServerInfo); !ok {
			c.errors <- fmt.Errorf("Expected op: '%s', got: %s",
				new(ServerInfo).Opcode(), op.Opcode())
			return
		} else {
			select {
			case c.ready <- info:
			default:
			}
		}

		q, ok := <-c.queries
		if !ok {
			break
		}
		if e := ops.Encode(encoder, q.request); e != nil {
			q.errors <- e
			close(q.errors)
			close(q.response)
			return
		}
		done := false

		for {
			op, err = ops.Decode(decoder)
			if err != nil {
				break
			}

			q.response <- op

			if _, ok := op.(Terminator); ok {
				done = true
				break
			}
		}

		if err != nil {
			q.errors <- err
		} else if !done {
			q.errors <- errors.New("connection closed unexpectedly")
		}

		close(q.errors)
		close(q.response)
	}
}

func (s *search) loop() {
	for o := range s.r.response {
		switch concrete := o.(type) {
		case *Result:
			s.results <- concrete
		case *ReplyError:
			s.err = QueryError{s.q, string(*concrete)}
		case *Stats:
			s.stats = concrete
		}
	}

	if s.err == nil {
		s.err = <-s.r.errors
	}

	close(s.results)
}

func (s *search) Results() <-chan *Result {
	return s.results
}

func (s *search) Close() (*Stats, error) {
	for _ = range s.results {
	}
	return s.stats, s.err
}
