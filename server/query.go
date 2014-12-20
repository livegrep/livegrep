package server

import (
	"regexp"
	"strings"

	"github.com/livegrep/livegrep/client"
)

var pieceRE = regexp.MustCompile(`\(|(?:^(\w+):)| `)

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

		// Three potentially-syntactically-meaningful cases:

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
			i := 0
			var r rune
			for i, r = range q {
				if r == '(' {
					p++
				} else if r == ')' {
					p--
				}
				if p == 0 {
					break
				}
			}
			ops[key] += match + q[:i]
			q = q[i:]
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
