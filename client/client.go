package client

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"strings"
)

type client struct {
	conn    net.Conn
	queries chan *search
	errors  chan error
	error   error
	ready   chan *ServerInfo
	info    *ServerInfo
}

type search struct {
	query   *Query
	results chan *Result
	errors  chan error
	stats   chan *Stats
}

func Dial(network, address string) (Client, error) {
	conn, err := net.Dial(network, address)
	if err != nil {
		return nil, err
	}
	cl := &client{
		conn:    conn,
		queries: make(chan *search),
		errors:  make(chan error, 1),
		ready:   make(chan *ServerInfo),
	}

	go cl.loop()

	select {
	case cl.info = <-cl.ready:
	case err = <-cl.errors:
		close(cl.queries)
		return nil, err
	}

	return cl, nil
}

func (c *client) Info() *ServerInfo {
	return c.info
}

func (c *client) Query(q *Query) (Search, error) {
	s := &search{q, make(chan *Result), make(chan error, 1), make(chan *Stats, 1)}
	select {
	case e := <-c.errors:
		return nil, e
	case c.queries <- s:
		return s, nil
	}
}

func (c *client) Close() {
	close(c.queries)
}

func (c *client) loop() {
	defer c.conn.Close()
	defer close(c.errors)
	scan := bufio.NewScanner(c.conn)
	encoder := json.NewEncoder(c.conn)

	for {
		if !scan.Scan() {
			c.errors <- scan.Err()
			return
		}
		if !bytes.HasPrefix(scan.Bytes(), []byte("READY ")) {
			c.errors <- fmt.Errorf("Expected READY, got: %s", scan.Text())
			return
		}

		info := &ServerInfo{}
		if err := json.Unmarshal(scan.Bytes()[len("READY "):], &info); err != nil {
			c.errors <- err
			return
		}

		select {
		case c.ready <- info:
		default:
		}

		q, ok := <-c.queries
		if !ok {
			break
		}
		if e := encoder.Encode(q.query); e != nil {
			q.errors <- e
			close(q.errors)
			close(q.results)
			close(q.stats)
			continue
		}
		for scan.Scan() {
			line := scan.Text()
			if strings.HasPrefix(line, "FATAL ") {
				q.errors <- QueryError{q.query, strings.TrimPrefix("FATAL ", line)}
				break
			} else if strings.HasPrefix(line, "DONE ") {
				stats := &Stats{}
				if e := json.Unmarshal(scan.Bytes()[len("DONE "):], stats); e != nil {
					q.errors <- e
				} else {
					q.stats <- stats
				}
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
		close(q.stats)

		if scan.Err() != nil {
			break
		}
	}
	if e := scan.Err(); e != nil && e != io.EOF {
		c.errors <- e
	}
}

func (s *search) Results() <-chan *Result {
	return s.results
}

func (s *search) Close() (*Stats, error) {
	for _ = range s.results {
	}
	return <-s.stats, <-s.errors
}
