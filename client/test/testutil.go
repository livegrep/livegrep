package test

import (
	"io"
	"os"
	"os/exec"

	"github.com/livegrep/livegrep/client"
)

type childClient struct {
	client.Client
	cmd  *exec.Cmd
	wait chan error
	err  error
}

func (c *childClient) Close() {
	c.Client.Close()
	if c.err != nil {
		return
	}
	<-c.wait
}

func (c *childClient) Err() error {
	select {
	case c.err = <-c.wait:
	default:
	}
	if c.err != nil {
		return c.err
	}
	if e := c.Client.Err(); e != nil {
		return e
	}
	return nil
}

type connection struct {
	in  io.ReadCloser
	out io.WriteCloser
}

func (c *connection) Read(b []byte) (int, error) {
	return c.in.Read(b)
}

func (c *connection) Write(b []byte) (int, error) {
	return c.out.Write(b)
}

func (c *connection) Close() error {
	e1 := c.in.Close()
	e2 := c.out.Close()
	if e1 != nil {
		return e1
	}
	if e2 != nil {
		return e2
	}
	return nil
}

const Codesearch = "../../bazel-bin/src/tools/codesearch"

func NewClient(args ...string) (client.Client, error) {
	cl := &childClient{wait: make(chan error)}
	cl.cmd = exec.Command(Codesearch, args...)

	cl.cmd.Stderr = os.Stderr
	in, e := cl.cmd.StdinPipe()
	if e != nil {
		return nil, e
	}
	out, e := cl.cmd.StdoutPipe()
	if e != nil {
		in.Close()
		return nil, e
	}

	if e = cl.cmd.Start(); e != nil {
		return nil, e
	}

	go func() {
		cl.wait <- cl.cmd.Wait()
	}()

	cl.Client, e = client.New(&connection{out, in})
	if e != nil {
		return nil, e
	}

	return cl, nil
}

// MockClient implements a fake Client that returns constant values
// based on the provided fields.
type MockClient struct {
	QueryError  error
	SearchError error
	Err_        error
	Info_       *client.ServerInfo
	Stats       *client.Stats
	Results     []*client.Result

	Closed  bool
	Queries []*client.Query
}

type mockSearch struct {
	m *MockClient
}

func (m *MockClient) Query(q *client.Query) (client.Search, error) {
	m.Queries = append(m.Queries, q)
	if m.QueryError != nil {
		return nil, m.QueryError
	}
	return &mockSearch{m}, nil
}

func (m *MockClient) Close() {
	m.Closed = true
}

func (m *MockClient) Info() *client.ServerInfo {
	return m.Info_
}

func (m *MockClient) Err() error {
	return m.Err_
}

func (s *mockSearch) Results() <-chan *client.Result {
	out := make(chan *client.Result, len(s.m.Results))
	for _, r := range s.m.Results {
		out <- r
	}
	close(out)
	return out
}

func (s *mockSearch) Close() (*client.Stats, error) {
	return s.m.Stats, s.m.SearchError
}
