package client

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"strings"
)

type Client struct {
	conn    net.Conn
	queries chan doQuery
	errors  chan error
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
		errors:  make(chan error),
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

func (c *Client) loop() {
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

		if e := scan.Err(); e != nil {
			c.errors <- e
			return
		}
	}
}
