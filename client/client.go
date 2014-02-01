package client

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"strings"
)

type Client struct {
	conn    net.Conn
	queries chan doQuery
	errors  chan error
	error   error
}

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

type doQuery struct {
	query   *Query
	results chan<- *Result
	errors  chan<- error
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

func Dial(network, address string) (*Client, error) {
	conn, err := net.Dial(network, address)
	if err != nil {
		return nil, err
	}
	cl := &Client{
		conn:    conn,
		queries: make(chan doQuery),
		errors:  make(chan error, 1),
	}

	go cl.loop()

	return cl, nil
}

func (c *Client) Query(q *Query) (chan *Result, chan error) {
	results := make(chan *Result)
	errors := make(chan error)
	c.queries <- doQuery{q, results, errors}
	return results, errors
}

func (c *Client) Close() {
	close(c.queries)
}

func (c *Client) Err() error {
	if c.error != nil {
		return c.error
	}
	select {
	case c.error = <-c.errors:
	default:
	}
	return c.error
}

func (c *Client) loop() {
	defer c.conn.Close()
	defer close(c.errors)
	scan := bufio.NewScanner(c.conn)
	if !scan.Scan() {
		c.errors <- scan.Err()
		return
	}
	if scan.Text() != "READY" {
		c.errors <- fmt.Errorf("Expected READY, got: %s", scan.Text())
		return
	}

	encoder := json.NewEncoder(c.conn)

	for q := range c.queries {
		if e := encoder.Encode(q.query); e != nil {
			q.errors <- e
			close(q.errors)
			close(q.results)
			continue
		}
		for scan.Scan() {
			line := scan.Text()
			if strings.HasPrefix(line, "FATAL ") {
				q.errors <- QueryError{q.query, strings.TrimPrefix("FATAL ", line)}
				break
			} else if strings.HasPrefix(line, "DONE ") {
				break
			} else {
				r := &Result{}
				if e := json.Unmarshal(scan.Bytes(), r); e != nil {
					q.errors <- e
					break
				}
				q.results <- r
			}
		}

		close(q.errors)
		close(q.results)

		if scan.Err() != nil {
			break
		}
	}
	if e := scan.Err(); e != nil && e != io.EOF {
		c.errors <- e
	}
}
