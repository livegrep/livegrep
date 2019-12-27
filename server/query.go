package server

import (
	"bytes"
	"errors"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"unicode/utf8"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
)

var pieceRE = regexp.MustCompile(`\[|\(|(?:^([a-zA-Z0-9-_]+):|\\.)| `)

var knownTags = map[string]bool{
	"file":        true,
	"-file":       true,
	"path":        true,
	"-path":       true,
	"repo":        true,
	"-repo":       true,
	"tags":        true,
	"-tags":       true,
	"case":        true,
	"lit":         true,
	"max_matches": true,
}

func onlyOneSynonym(ops map[string]string, op1 string, op2 string) (string, error) {
	if ops[op1] != "" && ops[op2] != "" {
		return "", fmt.Errorf("Cannot provide both %s: and %s:, because they are synonyms", op1, op2)
	}
	if ops[op1] != "" {
		return ops[op1], nil
	}
	return ops[op2], nil
}

func ParseQuery(query string, globalRegex bool) (pb.Query, error) {
	var out pb.Query

	ops := make(map[string]string)
	key := ""
	term := ""
	q := strings.TrimSpace(query)
	inRegex := globalRegex
	justGotSpace := true

	for {
		m := pieceRE.FindStringSubmatchIndex(q)
		if m == nil {
			term += q
			if _, alreadySet := ops[key]; alreadySet {
				return out, fmt.Errorf("got term twice: %s", key)
			}
			ops[key] = term
			break
		}

		term += q[:m[0]]
		match := q[m[0]:m[1]]
		q = q[m[1]:]

		justGotSpace = justGotSpace && m[0] == 0

		if match == " " {
			// A space: Ends the operator, if we're in one.
			if key == "" {
				term += " "

			} else {
				if _, alreadySet := ops[key]; alreadySet {
					return out, fmt.Errorf("got term twice: %s", key)
				}
				ops[key] = term
				key = ""
				term = ""
				inRegex = globalRegex
			}
		} else if match == "(" || match == "[" {
			if !(inRegex || justGotSpace) {
				term += match
			} else {
				// A parenthesis or a bracket. Consume
				// until the end of a balanced set.
				p := 1
				i := 0
				esc := false
				var w bytes.Buffer
				var open, close rune
				switch match {
				case "(":
					open, close = '(', ')'
				case "[":
					open, close = '[', ']'
				}

				for i < len(q) {
					// We decode runes ourselves instead
					// of using range because exiting the
					// loop with i = len(q) makes the edge
					// cases simpler.
					r, l := utf8.DecodeRuneInString(q[i:])
					i += l
					switch {
					case esc:
						esc = false
					case r == '\\':
						esc = true
					case r == open:
						p++
					case r == close:
						p--
					}
					w.WriteRune(r)
					if p == 0 {
						break
					}
				}
				term += match + w.String()
				q = q[i:]
			}
		} else if match[0] == '\\' {
			term += match
		} else {
			// An operator. The key is in match group 1
			newKey := match[m[2]-m[0] : m[3]-m[0]]
			if key == "" && knownTags[newKey] {
				if strings.TrimSpace(term) != "" {
					if _, alreadySet := ops[key]; alreadySet {
						return out, fmt.Errorf("main search term must be contiguous")
					}
					ops[key] = term
				}
				term = ""
				key = newKey
			} else {
				term += match
			}
			if key == "lit" {
				inRegex = false
			}
		}
		justGotSpace = (match == " ")
	}

	var err error
	if out.File, err = onlyOneSynonym(ops, "file", "path"); err != nil {
		return out, err
	}
	out.Repo = ops["repo"]
	out.Tags = ops["tags"]
	if out.NotFile, err = onlyOneSynonym(ops, "-file", "-path"); err != nil {
		return out, err
	}
	out.NotRepo = ops["-repo"]
	out.NotTags = ops["-tags"]
	var bits []string
	for _, k := range []string{"", "case", "lit"} {
		bit := strings.TrimSpace(ops[k])
		if k == "lit" || !globalRegex {
			bit = regexp.QuoteMeta(bit)
		}
		if len(bit) != 0 {
			bits = append(bits, bit)
		}
	}

	if len(bits) > 1 {
		return out, errors.New("You cannot provide multiple of case:, lit:, and a bare regex")
	}

	if len(bits) > 0 {
		out.Line = bits[0]
	}

	if !globalRegex {
		out.File = regexp.QuoteMeta(out.File)
		out.NotFile = regexp.QuoteMeta(out.NotFile)
		out.Repo = regexp.QuoteMeta(out.Repo)
		out.NotRepo = regexp.QuoteMeta(out.NotRepo)
	}

	if out.Line == "" && out.File != "" {
		out.Line = out.File
		out.File = ""
		out.FilenameOnly = true
	}

	if _, ok := ops["case"]; ok {
		out.FoldCase = false
	} else if _, ok := ops["lit"]; ok {
		out.FoldCase = false
	} else {
		out.FoldCase = strings.IndexAny(out.Line, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == -1
	}
	if v, ok := ops["max_matches"]; ok && v != "" {
		i, err := strconv.Atoi(v)
		if err == nil {
			out.MaxMatches = int32(i)
		} else {
			return out, errors.New("Value given to max_matches: must be a valid integer")
		}
	} else {
		out.MaxMatches = 0
	}

	return out, nil
}
