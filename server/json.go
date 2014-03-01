package server

import (
	"encoding/json"
)

type asJSON struct {
	v interface{}
}

func (j asJSON) String() string {
	b, e := json.Marshal(j.v)
	if e != nil {
		panic(e.Error())
	}
	return string(b)
}
