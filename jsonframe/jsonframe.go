package jsonframe

import (
	"encoding/json"
	"fmt"
	"reflect"
)

type UnknownOpcode struct {
	Opcode string
}

func (u *UnknownOpcode) Error() string {
	return fmt.Sprintf("unknown opcode '%s'", u.Opcode)
}

type Frame struct {
	Opcode string          `json:"opcode"`
	Body   json.RawMessage `json:"body"`
}

type wFrame struct {
	Opcode string      `json:"opcode"`
	Body   interface{} `json:"body"`
}

type Op interface {
	Opcode() string
}

type Marshaler struct {
	ops map[string]Op
}

func (m *Marshaler) Register(o Op) {
	if m.ops == nil {
		m.ops = make(map[string]Op)
	}
	if _, ok := m.ops[o.Opcode()]; ok {
		panic(fmt.Sprintf("Register: duplicate opcode: %s", o.Opcode()))
	}
	m.ops[o.Opcode()] = o
}

func (m *Marshaler) Encode(e *json.Encoder, op Op) error {
	frame := &wFrame{op.Opcode(), op}
	return e.Encode(frame)
}

func (m *Marshaler) Marshal(op Op) ([]byte, error) {
	frame := &wFrame{op.Opcode(), op}
	return json.Marshal(frame)
}

func (m *Marshaler) unpack(f *Frame, out *Op) error {
	prototype, ok := m.ops[f.Opcode]
	if !ok {
		return &UnknownOpcode{f.Opcode}
	}

	op := reflect.New(reflect.TypeOf(prototype).Elem()).Interface().(Op)
	if err := json.Unmarshal(f.Body, op); err != nil {
		return err
	}

	*out = op
	return nil
}

func (m *Marshaler) Decode(d *json.Decoder) (Op, error) {
	var f Frame
	if err := d.Decode(&f); err != nil {
		return nil, err
	}
	var o Op
	if err := m.unpack(&f, &o); err != nil {
		return nil, err
	}
	return o, nil
}

func (m *Marshaler) Unmarshal(buf []byte, out *Op) error {
	var f Frame
	if err := json.Unmarshal(buf, &f); err != nil {
		return err
	}
	return m.unpack(&f, out)
}
