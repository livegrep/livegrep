package server_test

import (
	"testing"

	"code.google.com/p/go.net/websocket"
	"github.com/nelhage/livegrep/jsonframe"
	"github.com/nelhage/livegrep/server"
	. "gopkg.in/check.v1"
)

func Test(t *testing.T) { TestingT(t) }

type ProtocolSuite struct{}

var _ = Suite(&ProtocolSuite{})

func (_ *ProtocolSuite) TestNoOpcode(c *C) {
	var op jsonframe.Op
	err := server.OpCodec.Unmarshal([]byte(`{"error": "monkeys"}`), websocket.TextFrame, &op)
	if err == nil {
		c.Error("Failed to return an error when unparsing with missing opcode.")
	}
}

func (_ *ProtocolSuite) TestRoundTrip(c *C) {
	op := &server.OpError{"error: fhqwgads"}
	data, payloadType, err := server.OpCodec.Marshal(op)
	if err != nil {
		c.Fatalf("marshal: %s", err.Error())
	}

	c.Logf("Marshalled: %s", data)

	var out jsonframe.Op
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
