package server_test

import (
	"github.com/nelhage/livegrep/server"
	. "launchpad.net/gocheck"
	"testing"
)

func Test(t *testing.T) { TestingT(t) }

type ProtocolSuite struct{}

var _ = Suite(&ProtocolSuite{})

func (_ *ProtocolSuite) TestRoundTrip(c *C) {
	op := &server.OpError{"error: fhqwgads"}
	data, payloadType, err := server.OpCodec.Marshal(op)
	if err != nil {
		c.Fatalf("marshal: %s", err.Error())
	}

	c.Logf("Marshalled: %s", data)

	var out server.Op
	err = server.OpCodec.Unmarshal(data, payloadType, &out)
	if err != nil {
		c.Fatalf("unmarshal: %s", err.Error())
	}

	switch t := out.(type) {
	case *server.OpError:
		c.Check(t.Error, Equals, op.Error)
	default:
		c.Fatalf("Unexpected type: %s", out)
	}
}
