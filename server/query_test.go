package server

import (
	"reflect"
	"testing"

	"github.com/livegrep/livegrep/client"
)

func TestParseQuery(t *testing.T) {
	cases := []struct {
		in  string
		out client.Query
	}{
		{
			"hello",
			client.Query{Line: "hello", FoldCase: true},
		},
		{
			"a b c",
			client.Query{Line: "a b c", FoldCase: true},
		},
		{
			"line file:.rb",
			client.Query{
				Line:     "line",
				File:     ".rb",
				FoldCase: true,
			},
		},
		{
			" a  ",
			client.Query{Line: "a", FoldCase: true},
		},
		{
			"( a  )",
			client.Query{Line: "( a  )", FoldCase: true},
		},
		{
			"Aa",
			client.Query{Line: "Aa", FoldCase: false},
		},
		{
			"case:abc",
			client.Query{Line: "abc", FoldCase: false},
		},
		{
			"case:abc file:^kernel/",
			client.Query{Line: "abc", FoldCase: false, File: "^kernel/"},
		},
		{
			"case:abc file:( )",
			client.Query{Line: "abc", FoldCase: false, File: "( )"},
		},
		{
			"a file:b c",
			client.Query{Line: "a c", FoldCase: true, File: "b"},
		},
		{
			"a file:((abc()())()) c",
			client.Query{Line: "a c", FoldCase: true, File: "((abc()())())"},
		},
		{
			"(  () (   ",
			client.Query{Line: "(  () (", FoldCase: true},
		},
	}

	for _, tc := range cases {
		parsed := ParseQuery(tc.in)
		if !reflect.DeepEqual(tc.out, parsed) {
			t.Errorf("error parsing %q: expected %#v got %#v",
				tc.in, tc.out, parsed)
		}
	}
}
