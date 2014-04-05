package client_test

import (
	"encoding/json"
	"github.com/nelhage/livegrep/client"
	"io"
	. "launchpad.net/gocheck"
	"net"
	"testing"
)

// We assume a codesearch running on localhost:9999. This could be
// improved.

func Test(t *testing.T) { TestingT(t) }

type ClientSuite struct {
	client client.Client
}

func (c *ClientSuite) TearDownTest(*C) {
	if c.client != nil {
		c.client.Close()
	}
}

var _ = Suite(&ClientSuite{})

type MockServer struct {
	Info    *client.ServerInfo
	Results []*client.Result
}

func (m *MockServer) handle(conn net.Conn) {
	defer conn.Close()

	encoder := json.NewEncoder(conn)
	reader := json.NewDecoder(conn)
	for {
		io.WriteString(conn, "READY ")
		encoder.Encode(m.Info)

		var q client.Query
		if err := reader.Decode(&q); err != nil {
			if err == io.EOF {
				return
			}
			panic(err.Error())
		}

		for _, r := range m.Results {
			encoder.Encode(r)
		}
		io.WriteString(conn, "DONE ")
		encoder.Encode(&client.Stats{})
	}
}

func runMockServer(handle func(net.Conn)) <-chan string {
	ready := make(chan string, 1)
	go func() {
		ln, err := net.Listen("tcp", ":0")
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

func (s *ClientSuite) connect(c *C, addr string) {
	var err error
	s.client, err = client.Dial("tcp", addr)
	if err != nil {
		c.Fatalf("connecting to %s: %s", addr, err.Error())
	}
}

func (s *ClientSuite) TestQuery(c *C) {
	s.connect(c, <-runMockServer((&MockServer{
		Results: []*client.Result{
			{Line: "match line 1"},
		},
	}).handle))
	search, err := s.client.Query(&client.Query{".", "", ""})
	c.Assert(err, IsNil)
	var n int
	for r := range search.Results() {
		n++
		c.Assert(r.Line, Not(Equals), "")
	}
	c.Assert(n, Equals, 1)
	st, e := search.Close()
	c.Assert(e, IsNil)
	c.Assert(st, Not(IsNil))
}

func (s *ClientSuite) TestTwoQueries(c *C) {
	s.connect(c, <-runMockServer((&MockServer{
		Results: []*client.Result{
			{Line: "match line 1"},
		},
	}).handle))

	search, err := s.client.Query(&client.Query{".", "", ""})
	c.Assert(err, IsNil)
	_, err = search.Close()
	c.Assert(err, IsNil)

	search, err = s.client.Query(&client.Query{".", "", ""})
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

type MockServerQueryError struct {
	Info *client.ServerInfo
	Err  string
}

func (m *MockServerQueryError) handle(conn net.Conn) {
	defer conn.Close()
	encoder := json.NewEncoder(conn)
	reader := json.NewDecoder(conn)
	for {
		io.WriteString(conn, "READY ")
		encoder.Encode(m.Info)

		var q client.Query
		if err := reader.Decode(&q); err != nil {
			if err == io.EOF {
				return
			}
			panic(err.Error())
		}

		io.WriteString(conn, "FATAL ")
		io.WriteString(conn, m.Err)
		io.WriteString(conn, "\n")
	}
}

func (s *ClientSuite) TestBadRegex(c *C) {
	errStr := "Invalid query: ("
	s.connect(c, <-runMockServer((&MockServerQueryError{
		Err: errStr,
	}).handle))

	search, err := s.client.Query(&client.Query{"(", "", ""})
	c.Assert(err, IsNil)
	for _ = range search.Results() {
		c.Fatal("Got back a result from an erroneous query!")
	}
	st, e := search.Close()
	c.Assert(st, IsNil)
	if e == nil {
		c.Fatal("Didn't get back an error")
	}
	if q, ok := e.(client.QueryError); ok {
		c.Assert(q.Query.Line, Equals, "(")
		c.Assert(q.Err, Equals, errStr)
	} else {
		c.Fatalf("Error %v wasn't a QueryError", e)
	}
}

func mockServerShutdown() <-chan string {
	return runMockServer(func(conn net.Conn) {
		conn.Write([]byte("READY {}\n"))
		conn.Close()
	})
}

func (s *ClientSuite) TestShutdown(c *C) {
	addr := <-mockServerShutdown()

	s.connect(c, addr)

	search, err := s.client.Query(&client.Query{Line: "l"})
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

	search, err = s.client.Query(&client.Query{Line: "l"})
	c.Assert(err, Not(IsNil))
	c.Assert(search, IsNil)
}
