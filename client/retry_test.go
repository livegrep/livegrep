package client_test

import (
	"errors"
	"github.com/nelhage/livegrep/client"
	. "launchpad.net/gocheck"
)

type RetrySuite struct {
	client client.Client
}

var _ = Suite(&RetrySuite{})

func (s *RetrySuite) TestAlwaysError(c *C) {
	theError := errors.New("No client here")
	cl := client.ClientWithRetry(func() (client.Client, error) { return nil, theError })
	c.Assert(cl, Not(IsNil))

	search, e := cl.Query(&client.Query{Line: "line"})
	c.Assert(search, IsNil)
	c.Assert(e, Equals, theError)
}
