package server

import (
	"code.google.com/p/go.net/websocket"
	"encoding/json"
	"fmt"
	"github.com/nelhage/livegrep/client"
	"reflect"
)

type WebsocketFrame struct {
	Opcode string          `json:"opcode"`
	Body   json.RawMessage `json:"body"`
}

type OpError struct {
	Error string `json:"error"`
}

func (o *OpError) Opcode() string { return "error" }

type OpQuery struct {
	Id      int64  `json:"id"`
	Line    string `json:"line"`
	File    string `json:"file"`
	Repo    string `json:"repo"`
	Backend string `json:"backend"`
}

func (o *OpQuery) Opcode() string { return "query" }

type OpResult struct {
	Search int64          `json:"id"`
	Result *client.Result `json:"result"`
}

func (o *OpResult) Opcode() string { return "result" }

type OpQueryError struct {
	Search int64  `json:"id"`
	Error  string `json:"error"`
}

func (o *OpQueryError) Opcode() string { return "query_error" }

type Op interface {
	Opcode() string
}

var opTable map[string]Op

func registerOp(o Op) {
	opTable[o.Opcode()] = o
}
func init() {
	opTable = make(map[string]Op)
	registerOp(&OpError{})
	registerOp(&OpQuery{})
	registerOp(&OpResult{})
	registerOp(&OpQueryError{})
}

func marshalOp(v interface{}) (data []byte, payloadType byte, err error) {
	op, ok := v.(Op)
	if !ok {
		panic("marshalOp: Must provide an implementation of Op.")
	}
	inner, err := json.Marshal(op)
	if err != nil {
		return
	}
	frame := &WebsocketFrame{
		Opcode: op.Opcode(),
		Body:   json.RawMessage(inner),
	}
	data, err = json.Marshal(frame)
	payloadType = websocket.TextFrame
	return
}

func unmarshalOp(data []byte, payloadType byte, v interface{}) (err error) {
	val := reflect.ValueOf(v)
	if val.Type().Kind() != reflect.Ptr {
		panic("unmarshalOp: Must provide a pointer type")
	}

	var frame WebsocketFrame
	if err = json.Unmarshal(data, &frame); err != nil {
		return
	}

	prototype, ok := opTable[frame.Opcode]
	if !ok {
		return fmt.Errorf("Unknown opcode %s", frame.Opcode)
	}

	op := reflect.New(reflect.TypeOf(prototype).Elem()).Interface().(Op)
	if err = json.Unmarshal(frame.Body, op); err != nil {
		return err
	}

	val.Elem().Set(reflect.ValueOf(op))
	return nil
}

var OpCodec = websocket.Codec{marshalOp, unmarshalOp}
