package server

import (
	"code.google.com/p/go.net/websocket"
	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/jsonframe"
)

type OpError struct {
	Error string `json:"error"`
}

func (o *OpError) Opcode() string { return "error" }

type OpQuery struct {
	Id       int64  `json:"id"`
	Line     string `json:"line"`
	File     string `json:"file"`
	Repo     string `json:"repo"`
	Backend  string `json:"backend"`
	FoldCase bool   `json:"fold_case"`
}

func (o *OpQuery) Opcode() string { return "query" }

type OpResult struct {
	Search int64          `json:"id"`
	Result *client.Result `json:"result"`
}

func (o *OpResult) Opcode() string { return "result" }

type OpSearchDone struct {
	Search   int64         `json:"id"`
	Duration int64         `json:"time"`
	Stats    *client.Stats `json:"stats"`
}

func (o *OpSearchDone) Opcode() string { return "search_done" }

type OpQueryError struct {
	Search int64  `json:"id"`
	Error  string `json:"error"`
}

func (o *OpQueryError) Opcode() string { return "query_error" }

var websocketOps jsonframe.Marshaler

func registerOp(o jsonframe.Op) {
	websocketOps.Register(o)
}
func init() {
	registerOp(&OpError{})
	registerOp(&OpQuery{})
	registerOp(&OpResult{})
	registerOp(&OpSearchDone{})
	registerOp(&OpQueryError{})
}

func marshalOp(v interface{}) (data []byte, payloadType byte, err error) {
	op, ok := v.(jsonframe.Op)
	if !ok {
		panic("marshalOp: Must provide an implementation of Op.")
	}
	data, err = websocketOps.Marshal(op)
	payloadType = websocket.TextFrame
	return
}

type ProtocolError struct {
	Inner error
}

func (pe *ProtocolError) Error() string {
	return pe.Inner.Error()
}

func unmarshalOp(data []byte, payloadType byte, v interface{}) (err error) {
	op := v.(*jsonframe.Op)
	if op == nil {
		panic("unmarshalOp: Must provide a non-nil Op*")
	}

	if e := websocketOps.Unmarshal(data, op); e != nil {
		return &ProtocolError{e}
	}
	return nil
}

var OpCodec = websocket.Codec{marshalOp, unmarshalOp}
