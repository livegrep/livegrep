package client

import (
	"encoding/json"
	"io"
	"net"
	"strings"
	"testing"

	"gopkg.in/check.v1"
)

func Test(t *testing.T) { check.TestingT(t) }

type ClientSuite struct {
	client Client
}

func (c *ClientSuite) TearDownTest(*check.C) {
	if c.client != nil {
		c.client.Close()
	}
}

var _ = check.Suite(&ClientSuite{})

var (
	Equals = check.Equals
	IsNil  = check.IsNil
	Not    = check.Not
)

type MockServer struct {
	Info    *ServerInfo
	Results []*Result
}

func (m *MockServer) handle(conn net.Conn) {
	defer conn.Close()

	encoder := json.NewEncoder(conn)
	decoder := json.NewDecoder(conn)
	for {
		ops.Encode(encoder, m.Info)

		if op, err := ops.Decode(decoder); err != nil {
			if err == io.EOF {
				return
			}
			panic(err.Error())
		} else {
			if op.(*Query) == nil {
				panic("expected query")
			}
		}

		for _, r := range m.Results {
			ops.Encode(encoder, r)
		}

		ops.Encode(encoder, &Stats{})
	}
}

func runMockServer(handle func(net.Conn)) <-chan string {
	ready := make(chan string, 1)
	go func() {
		defer close(ready)
		ln, err := net.Listen("tcp4", ":0")
		if err != nil {
			panic(err.Error())
		}
		defer ln.Close()
		ready <- ln.Addr().String()
		conn, err := ln.Accept()
		if err != nil {
			panic(err.Error())
		}
		handle(conn)
	}()
	return ready
}

func (s *ClientSuite) connect(c *check.C, addr string) {
	var err error
	s.client, err = Dial("tcp", addr)
	if err != nil {
		c.Fatalf("connecting to %s: %s", addr, err.Error())
	}
}

func (s *ClientSuite) TestQuery(c *check.C) {
	s.connect(c, <-runMockServer((&MockServer{
		Results: []*Result{
			{Line: "match line 1"},
		},
	}).handle))
	search, err := s.client.Query(&Query{Line: "."})
	c.Assert(err, check.IsNil)
	var n int
	for r := range search.Results() {
		n++
		c.Assert(r.Line, check.Not(Equals), "")
	}
	c.Assert(n, Equals, 1)
	st, e := search.Close()
	c.Assert(e, IsNil)
	c.Assert(st, Not(IsNil))
}

func (s *ClientSuite) TestTwoQueries(c *check.C) {
	s.connect(c, <-runMockServer((&MockServer{
		Results: []*Result{
			{Line: "match line 1"},
		},
	}).handle))

	search, err := s.client.Query(&Query{Line: "."})
	c.Assert(err, IsNil)
	_, err = search.Close()
	c.Assert(err, IsNil)

	search, err = s.client.Query(&Query{Line: "."})
	c.Assert(err, IsNil)
	n := 0
	for _ = range search.Results() {
		n++
	}
	_, err = search.Close()
	if err != nil {
		c.Fatalf("Unexpected error: %s", err.Error())
	}
	c.Assert(n, Not(Equals), 0)
}

func (s *ClientSuite) TestLongLine(c *check.C) {
	longLine := strings.Repeat("X", 1<<16)
	s.connect(c, <-runMockServer((&MockServer{
		Results: []*Result{
			{Line: longLine},
		},
	}).handle))
	search, err := s.client.Query(&Query{Line: "."})
	c.Assert(err, IsNil)
	var rs []*Result
	for r := range search.Results() {
		rs = append(rs, r)
	}

	c.Assert(len(rs), Equals, 1)
	c.Assert(rs[0].Line, Equals, longLine)
}

type MockServerQueryError struct {
	Info *ServerInfo
	Err  string
}

func (m *MockServerQueryError) handle(conn net.Conn) {
	defer conn.Close()
	encoder := json.NewEncoder(conn)
	decoder := json.NewDecoder(conn)
	for {
		ops.Encode(encoder, m.Info)

		if op, err := ops.Decode(decoder); err != nil {
			if err == io.EOF {
				return
			}
			panic(err.Error())
		} else {
			if op.(*Query) == nil {
				panic("expected query")
			}
		}

		re := ReplyError(m.Err)
		ops.Encode(encoder, &re)
	}
}

func (s *ClientSuite) TestBadRegex(c *check.C) {
	errStr := "Invalid query: ("
	s.connect(c, <-runMockServer((&MockServerQueryError{
		Err: errStr,
	}).handle))

	search, err := s.client.Query(&Query{Line: "("})
	c.Assert(err, IsNil)
	for _ = range search.Results() {
		c.Fatal("Got back a result from an erroneous query!")
	}
	st, e := search.Close()
	c.Assert(st, IsNil)
	if e == nil {
		c.Fatal("Didn't get back an error")
	}
	if q, ok := e.(QueryError); ok {
		c.Assert(q.Query.Line, Equals, "(")
		c.Assert(q.Err, Equals, errStr)
	} else {
		c.Fatalf("Error %v wasn't a QueryError", e)
	}
}

func mockServerShutdown() <-chan string {
	return runMockServer(func(conn net.Conn) {
		ops.Encode(json.NewEncoder(conn), &ServerInfo{})
		conn.Close()
	})
}

func (s *ClientSuite) TestShutdown(c *check.C) {
	addr := <-mockServerShutdown()

	s.connect(c, addr)

	search, err := s.client.Query(&Query{Line: "l"})
	c.Assert(err, IsNil)
	c.Assert(search, Not(IsNil))

	results := search.Results()
	c.Assert(results, Not(IsNil))
	for r := range results {
		c.Errorf("Got a result back: %+v", r)
	}
	st, err := search.Close()
	c.Assert(st, IsNil)
	c.Assert(err, Not(IsNil))

	search, err = s.client.Query(&Query{Line: "l"})
	c.Assert(err, Not(IsNil))
	c.Assert(search, IsNil)
}
