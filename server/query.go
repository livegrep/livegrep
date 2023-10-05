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

func ensureSingleValue(ops map[string][]string, key string) (string, error) {
	if len(ops[key]) > 1 {
		return "", fmt.Errorf("multiple values for %s:", key)
	}
	if len(ops[key]) == 1 {
		return ops[key][0], nil
	}
	return "", nil
}

func ParseQuery(query string, globalRegex bool) (pb.Query, error) {
	var out pb.Query

	ops := make(map[string][]string)
	key := ""
	term := ""
	q := strings.TrimSpace(query)
	inRegex := globalRegex
	justGotSpace := true

	for {
		m := pieceRE.FindStringSubmatchIndex(q)
		if m == nil {
			term += q
			ops[key] = append(ops[key], term)
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
				ops[key] = append(ops[key], term)
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
					ops[key] = append(ops[key], term)
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

	// This is a special case to provide a better error message,
	// since the main search term is represented by the "" op.
	if len(ops[""]) > 1 {
		return out, fmt.Errorf("main search term must be contiguous")
	}

	// Handle synonyms
	out.File = append(ops["file"], ops["path"]...)
	out.NotFile = append(ops["-file"], ops["-path"]...)

	var err error
	out.Repo, err = ensureSingleValue(ops, "repo")
	if err != nil {
		return out, err
	}
	out.Tags, err = ensureSingleValue(ops, "tags")
	if err != nil {
		return out, err
	}
	out.NotRepo, err = ensureSingleValue(ops, "-repo")
	if err != nil {
		return out, err
	}
	out.NotTags, err = ensureSingleValue(ops, "-tags")
	if err != nil {
		return out, err
	}
	var bits []string
	for _, k := range []string{"", "case", "lit"} {
		if _, ok := ops[k]; !ok {
			continue
		}
		bit := strings.TrimSpace(ops[k][0])
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
		for i, f := range out.File {
			out.File[i] = regexp.QuoteMeta(f)
		}
		for i, f := range out.NotFile {
			out.NotFile[i] = regexp.QuoteMeta(f)
		}
		out.Repo = regexp.QuoteMeta(out.Repo)
		out.NotRepo = regexp.QuoteMeta(out.NotRepo)
	}

	if len(out.Line) == 0 && len(out.File) != 0 {
		// setting Line for a FilenameOnly search is a slight hack.
		// it has compatibility with older backend code that expects a
		// single filename regex, but a newer backend will use it to
		// determine which match is highlighted.
		out.Line = out.File[0]
		out.FilenameOnly = true
	}

	if _, ok := ops["case"]; ok {
		out.FoldCase = false
	} else if _, ok := ops["lit"]; ok {
		out.FoldCase = false
	} else {
		out.FoldCase = strings.IndexAny(out.Line, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == -1
	}
	if v, ok := ops["max_matches"]; ok && v[0] != "" {
		v := v[0]
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
