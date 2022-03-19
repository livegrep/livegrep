package server

import (
	"encoding/json"
	"reflect"
	"testing"
)

func TestReadmeRegex(t *testing.T) {
	cases := []struct {
		in  string
		out []string
	}{
		{
			"README.md",
			[]string{"README.md", "README", "md"},
		},
		{
			"readme.md",
			[]string{"readme.md", "readme", "md"},
		},
		{
			"readme.rst",
			[]string{"readme.rst", "readme", "rst"},
		},
		{
			"readme.unknown",
			nil,
		},
		{
			"what.md",
			nil,
		},
	}

	for _, tc := range cases {
		matches := supportedReadmeRegex.FindStringSubmatch(tc.in)
		if !reflect.DeepEqual(tc.out, matches) {
			got, _ := json.MarshalIndent(matches, "", "  ")
			want, _ := json.MarshalIndent(tc.out, "", "  ")
			t.Errorf("error parsing %q: expected:\n%s\ngot:\n%s",
				tc.in, want, got)
		}
	}
}
