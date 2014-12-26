package server

import (
	"bytes"
	"regexp"
	"strings"
	"unicode/utf8"

	"github.com/livegrep/livegrep/client"
)

var pieceRE = regexp.MustCompile(`\(|(?:^(\w+):|\\.)| `)

func ParseQuery(query string) client.Query {
	ops := make(map[string]string)
	key := ""
	q := strings.TrimSpace(query)

	for {
		m := pieceRE.FindStringSubmatchIndex(q)
		if m == nil {
			ops[key] += q
			break
		}

		ops[key] += q[:m[0]]
		match := q[m[0]:m[1]]
		q = q[m[1]:]

		if match == " " {
			// A space: Ends the operator, if we're in one.
			if key == "" {
				ops[key] += " "
			} else {
				key = ""
			}
		} else if match == "(" {
			// A parenthesis. Nothing is special until the
			// end of a balanced set of parenthesis
			p := 1
			esc := false
			var (
				i int
				r rune
			)
			var w bytes.Buffer
			for i, r = range q {
				switch {
				case esc:
					esc = false
				case r == '\\':
					esc = true
				case r == '(':
					p++
				case r == ')':
					p--
				}
				w.WriteRune(r)
				if p == 0 {
					break
				}
			}
			ops[key] += match + w.String()
			q = q[i+utf8.RuneLen(r):]
		} else if match[0] == '\\' {
			ops[key] += match
		} else {
			// An operator. The key is in match group 1
			key = match[m[2]-m[0] : m[3]-m[0]]
		}
	}

	var out client.Query
	out.File = ops["file"]
	out.Repo = ops["repo"]
	out.Line = strings.TrimSpace(ops[""] + ops["case"])
	if _, ok := ops["case"]; ok {
		out.FoldCase = false
	} else {
		out.FoldCase = strings.IndexAny(out.Line, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == -1
	}

	return out
}
