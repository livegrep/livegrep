package api

import "github.com/nelhage/livegrep/client"

type InnerError struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

// ReplyError is returned along with any non-200 status reply
type ReplyError struct {
	Err InnerError `json:"error"`
}

// ReplySearch is returned to /api/v1/search/:backend
type ReplySearch struct {
	Info    *client.Stats    `json:"info"`
	Results []*client.Result `json:"results"`
}
