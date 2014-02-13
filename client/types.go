package client

import (
	"fmt"
)

type Query struct {
	Line string `json:"line"`
	File string `json:"file"`
	Repo string `json:"repo"`
}

type QueryError struct {
	Query *Query
	Err   string
}

func (q QueryError) Error() string {
	return fmt.Sprintf("Query Error: %s", q.Err)
}

type Result struct {
	Contexts []struct {
		Paths []struct {
			Ref  string `json:"ref"`
			Path string `json:"path"`
		} `json:"paths"`
		LineNumber    int      `json:"lno"`
		ContextBefore []string `json:"context_before"`
		ContextAfter  []string `json:"context_after"`
	} `json:"contexts"`

	Bounds [2]int `json:"bounds"`
	Line   string `json:"line"`
}

type Stats struct {
	RE2Time     int64  `json:"re2_time"`
	GitTime     int64  `json:"git_time"`
	SortTime    int64  `json:"sort_time"`
	IndexTime   int64  `json:"index_time"`
	AnalyzeTime int64  `json:"analyze_time"`
	ExitReason  string `json:"why"`
}

type ServerInfo struct {
	Name  string   `json:"name"`
	Trees []string `json:"trees"`
}

type Client interface {
	Query(q *Query) (Search, error)
	Close()
}

type Search interface {
	Results() <-chan *Result
	Close() (*Stats, error)
}
