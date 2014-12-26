package server

import (
	"errors"
	"testing"
	"time"

	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/client/test"
)

func init() {
	maxBackoff = 1 * time.Millisecond
}

func TestFailedDial(t *testing.T) {
	tries := 0
	dial := func() (client.Client, error) {
		tries++
		if tries < 10 {
			return nil, errors.New("connect error")
		}
		return &test.MockClient{
			QueryError: errors.New("query failed"),
		}, nil
	}

	bk := &Backend{
		Id:       "search",
		Dial:     dial,
		PoolSize: 2,
	}

	bk.Start()
	defer bk.Close()

	select {
	case <-time.After(1 * time.Second):
		t.Fatal("timed out waiting for dial")
	case <-bk.Clients:
	}

}

func assertClosed(t *testing.T, bk *Backend) {
	select {
	case _, ok := <-bk.pending:
		if ok {
			t.Fatal("pending data")
		}
	default:
		t.Fatal("pending not closed")
	}
	select {
	case _, ok := <-bk.Clients:
		if ok {
			t.Fatal("clients data")
		}
	default:
		t.Fatal("clients not closed")
	}
}

func TestCloseClients(t *testing.T) {
	var clients []*test.MockClient
	dial := func() (client.Client, error) {
		cl := &test.MockClient{
			QueryError: errors.New("query failed"),
		}
		clients = append(clients, cl)
		return cl, nil
	}
	bk := &Backend{
		Dial: dial,
		Id:   "q",
	}
	bk.Start()
	for i := 0; i < bk.PoolSize; i++ {
		cl := <-bk.Clients
		bk.CheckIn(cl)
	}
	bk.Close()
	for i, cl := range clients {
		if !cl.Closed {
			t.Fatal("failed to close", i)
		}
	}
	assertClosed(t, bk)
}

func TestCloseNoClients(t *testing.T) {
	dial := func() (client.Client, error) {
		return nil, errors.New("error")
	}
	bk := &Backend{
		Dial: dial,
		Id:   "q",
	}
	bk.Start()
	bk.Close()
	assertClosed(t, bk)
}
