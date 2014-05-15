package client

type Query struct {
	Line     string `json:"line"`
	File     string `json:"file"`
	Repo     string `json:"repo"`
	FoldCase bool   `json:"fold_case"`
}

func (q *Query) Opcode() string {
	return "query"
}

type QueryError struct {
	Query *Query
	Err   string
}

func (q QueryError) Error() string {
	return q.Err
}

type Result struct {
	Contexts []struct {
		Paths []struct {
			Repo string `json:"repo"`
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

func (r *Result) Opcode() string {
	return "match"
}

type Stats struct {
	RE2Time     int64  `json:"re2_time"`
	GitTime     int64  `json:"git_time"`
	SortTime    int64  `json:"sort_time"`
	IndexTime   int64  `json:"index_time"`
	AnalyzeTime int64  `json:"analyze_time"`
	ExitReason  string `json:"why"`
}

func (s *Stats) Opcode() string {
	return "done"
}

type ServerInfo struct {
	Name  string `json:"name"`
	Repos []struct {
		Name     string                 `json:"name"`
		Metadata map[string]interface{} `json:"metadata"`
	} `json:"repos"`
}

func (s *ServerInfo) Opcode() string {
	return "ready"
}

type ReplyError string

func (r *ReplyError) Opcode() string {
	return "error"
}

type Client interface {
	Query(q *Query) (Search, error)
	Close()
	Info() *ServerInfo
	Err() error
}

type Search interface {
	Results() <-chan *Result
	Close() (*Stats, error)
}
