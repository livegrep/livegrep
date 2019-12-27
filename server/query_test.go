package server

import (
	"encoding/json"
	"reflect"
	"testing"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
)

func TestParseQuery(t *testing.T) {
	cases := []struct {
		in    string
		out   pb.Query
		regex bool
	}{
		// regex parse mode
		{
			"hello",
			pb.Query{Line: "hello", FoldCase: true},
			true,
		},
		{
			"a b c",
			pb.Query{Line: "a b c", FoldCase: true},
			true,
		},
		{
			"line file:.rb",
			pb.Query{
				Line:     "line",
				File:     ".rb",
				FoldCase: true,
			},
			true,
		},
		{
			" a  ",
			pb.Query{Line: "a", FoldCase: true},
			true,
		},
		{
			"( a  )",
			pb.Query{Line: "( a  )", FoldCase: true},
			true,
		},
		{
			"Aa",
			pb.Query{Line: "Aa", FoldCase: false},
			true,
		},
		{
			"case:abc",
			pb.Query{Line: "abc", FoldCase: false},
			true,
		},
		{
			"case:abc file:^kernel/",
			pb.Query{Line: "abc", FoldCase: false, File: "^kernel/"},
			true,
		},
		{
			"case:abc file:( )",
			pb.Query{Line: "abc", FoldCase: false, File: "( )"},
			true,
		},
		{
			"(  () (   ",
			pb.Query{Line: "(  () (", FoldCase: true},
			true,
		},
		{
			`a file:\(`,
			pb.Query{Line: "a", File: `\(`, FoldCase: true},
			true,
		},
		{
			`a file:(\()`,
			pb.Query{Line: "a", File: `(\()`, FoldCase: true},
			true,
		},
		{
			`(`,
			pb.Query{Line: "(", FoldCase: true},
			true,
		},
		{
			`(file:)`,
			pb.Query{Line: "(file:)", FoldCase: true},
			true,
		},
		{
			`re tags:kind:function`,
			pb.Query{Line: "re", FoldCase: true, Tags: "kind:function"},
			true,
		},
		{
			`-file:Godep re`,
			pb.Query{Line: "re", NotFile: "Godep", FoldCase: true},
			true,
		},
		{
			`-file:. -repo:Godep re`,
			pb.Query{Line: "re", NotFile: ".", NotRepo: "Godep", FoldCase: true},
			true,
		},
		{
			`-tags:kind:class re`,
			pb.Query{Line: "re", NotTags: "kind:class", FoldCase: true},
			true,
		},
		{
			`case:foo:`,
			pb.Query{Line: "foo:", FoldCase: false},
			true,
		},
		{
			`lit:.`,
			pb.Query{Line: `\.`, FoldCase: false},
			true,
		},
		{
			`std::string`,
			pb.Query{Line: `std::string`, FoldCase: true},
			true,
		},
		{
			`a max_matches:100`,
			pb.Query{Line: "a", FoldCase: true, MaxMatches: 100},
			true,
		},
		{
			`a max_matches:`,
			pb.Query{Line: "a", FoldCase: true},
			true,
		},
		{
			`file:hello`,
			pb.Query{Line: "hello", FoldCase: true, FilenameOnly: true},
			true,
		},
		{
			`file:HELLO`,
			pb.Query{Line: "HELLO", FoldCase: false, FilenameOnly: true},
			true,
		},
		{
			`lit:a( file:b`,
			pb.Query{Line: `a\(`, File: "b", FoldCase: false},
			true,
		},
		{
			`lit:a(b file:c`,
			pb.Query{Line: `a\(b`, File: "c", FoldCase: false},
			true,
		},
		{
			`[(] file:\.c`,
			pb.Query{Line: `[(]`, File: "\\.c", FoldCase: true},
			true,
		},
		{
			`[ ] file:\.c`,
			pb.Query{Line: `[ ]`, File: "\\.c", FoldCase: true},
			true,
		},
		{
			`[ \]] file:\.c`,
			pb.Query{Line: `[ \]]`, File: "\\.c", FoldCase: true},
			true,
		},

		// literal parse mode
		{
			"a( file:b",
			pb.Query{Line: `a\(`, File: "b", FoldCase: true},
			false,
		},
		{
			"a (file:b",
			pb.Query{Line: `a \(file:b`, FoldCase: true},
			false,
		},
		{
			"(file:a b",
			pb.Query{Line: `\(file:a b`, FoldCase: true},
			false,
		},
		{
			"(file:a) b",
			pb.Query{Line: `\(file:a\) b`, FoldCase: true},
			false,
		},
		{
			"(file:a repo:b",
			pb.Query{Line: `\(file:a repo:b`, FoldCase: true},
			false,
		},
		{
			"(file:a) repo:b",
			pb.Query{Line: `\(file:a\)`, Repo: "b", FoldCase: true},
			false,
		},
		{
			"(file:a) (repo:b)",
			pb.Query{Line: `\(file:a\) \(repo:b\)`, FoldCase: true},
			false,
		},
		{
			"file:a( b",
			pb.Query{Line: `b`, File: `a\(`, FoldCase: true},
			false,
		},
	}

	for _, tc := range cases {
		parsed, err := ParseQuery(tc.in, tc.regex)
		if !reflect.DeepEqual(tc.out, parsed) {
			got, _ := json.MarshalIndent(parsed, "", "  ")
			want, _ := json.MarshalIndent(tc.out, "", "  ")
			t.Errorf("error parsing %q: expected:\n%s\ngot:\n%s",
				tc.in, want, got)
		}
		if err != nil {
			t.Errorf("parse(%v) error=%v", tc.in, err)
		}
	}
}

func TestParseQueryError(t *testing.T) {
	cases := []struct {
		in string
	}{
		{"case:a b"},
		{"lit:a b"},
		{"case:a lit:b"},
		{"a max_matches:a"},
		{"a file:b c"},
		{"a file:((abc()())()) c"},
	}

	for _, tc := range cases {
		parsed, err := ParseQuery(tc.in, true)
		if err == nil {
			t.Errorf("expected an error parsing (%v), got %#v", tc.in, parsed)
		}
	}
}
