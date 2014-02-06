package client_test

import (
	"github.com/nelhage/livegrep/client"
	. "launchpad.net/gocheck"
	"testing"
)

// We assume a codesearch running on localhost:9999. This could be
// improved.

func Test(t *testing.T) { TestingT(t) }

type ClientSuite struct {
	client client.Client
}

var _ = Suite(&ClientSuite{})

func (s *ClientSuite) SetUpTest(c *C) {
	var err error
	s.client, err = client.Dial("tcp", "localhost:9999")
	if err != nil {
		panic(err.Error())
	}
}

func (s *ClientSuite) TestQuery(c *C) {
	search := s.client.Query(&client.Query{".", "", ""})
	var n int
	for r := range search.Results() {
		n++
		c.Assert(r.Line, Not(Equals), "")
	}
	c.Assert(n, Not(Equals), 0)
	st, e := search.Close()
	c.Assert(st, Not(IsNil))
	c.Assert(e, IsNil)
}

func (s *ClientSuite) TestBadRegex(c *C) {
	search := s.client.Query(&client.Query{"(", "", ""})
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
	} else {
		c.Fatalf("Error %v wasn't a QueryError", e)
	}
}
