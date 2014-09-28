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
	queries chan *search
	errors  chan error
	error   error
	ready   chan *ServerInfo
	info    *ServerInfo
}

type search struct {
	query   *Query
	results chan *Result
	errors  chan error
	stats   chan *Stats
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
		queries: make(chan *search),
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
	s := &search{q, make(chan *Result), make(chan error, 1), make(chan *Stats, 1)}
	select {
	case e, ok := <-c.errors:
		if !ok {
			e = errors.New("use of a closed Client")
		}
		c.error = e
		return nil, e
	case c.queries <- s:
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
		if e := ops.Encode(encoder, q.query); e != nil {
			q.errors <- e
			close(q.errors)
			close(q.results)
			close(q.stats)
			continue
		}
		done := false
	ResultLoop:
		for {
			op, err = ops.Decode(decoder)
			if err != nil {
				break
			}
			switch concrete := op.(type) {
			case *ReplyError:
				q.errors <- QueryError{q.query, string(*concrete)}
				done = true
				break ResultLoop
			case *Stats:
				q.stats <- concrete
				done = true
				break ResultLoop
			case *Result:
				q.results <- concrete
			}
		}

		if err != nil {
			q.errors <- err
		} else if !done {
			q.errors <- errors.New("connection closed unexpectedly")
		}

		close(q.errors)
		close(q.results)
		close(q.stats)
	}
}

func (s *search) Results() <-chan *Result {
	return s.results
}

func (s *search) Close() (*Stats, error) {
	for _ = range s.results {
	}
	return <-s.stats, <-s.errors
}
