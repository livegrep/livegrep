package test

import (
	"errors"
	"reflect"
	"testing"

	"github.com/livegrep/livegrep/client"
)

func TestQueryError(t *testing.T) {
	m := &MockClient{QueryError: errors.New("query error")}
	s, e := m.Query(&client.Query{})
	if e == nil || s != nil {
		t.Fatalf("QueryError")
	}
	if e.Error() != "query error" {
		t.Fatalf("QueryError %s", e)
	}
}

func TestSearchError(t *testing.T) {
	m := &MockClient{SearchError: errors.New("search error")}
	s, e := m.Query(&client.Query{})
	if e != nil || s == nil {
		t.Fatal("SearchError")
	}
	r := s.Results()
	if _, ok := <-r; ok {
		t.Fatal("SearchError results")
	}
	_, e = s.Close()
	if e == nil {
		t.Fatal("SearchError error")
	}
	if e.Error() != "search error" {
		t.Fatal("SearchErrors", e)
	}
}

func TestErr(t *testing.T) {
	m := &MockClient{Err_: errors.New("client error")}
	e := m.Err()
	if e == nil || e.Error() != "client error" {
		t.Fatal("Err", e)
	}
}

func TestResults(t *testing.T) {
	rs := []*client.Result{
		{Line: "line 1"},
		{Line: "line 2"},
	}
	st := &client.Stats{ExitReason: "time"}
	m := &MockClient{Results: rs, Stats: st}
	s, e := m.Query(&client.Query{})
	if s == nil || e != nil {
		t.Fatal("Results", s, e)
	}
	rc := s.Results()
	i := 0
	for r := range rc {
		if !reflect.DeepEqual(r, rs[i]) {
			t.Fatal("result", i, r)
		}
		i++
	}
	if i != len(rs) {
		t.Fatalf("len %d!=%d", len(rs), i)
	}
	st_, e := s.Close()
	if st_ != st || e != nil {
		t.Fatal("Results")
	}
}
