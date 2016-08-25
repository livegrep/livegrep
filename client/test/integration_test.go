package test

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"reflect"
	"regexp"
	"sort"
	"strconv"
	"testing"

	"gopkg.in/check.v1"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
)

const LineLimit = 1024

func Test(t *testing.T) { check.TestingT(t) }

var repo = flag.String("test.repo", "", "Git repository to run integration tests against.")
var patterns = flag.String("test.patterns", "patterns", "File containing patterns for integration testing.")

type IntegrationSuite struct {
	config   *os.File
	index    *os.File
	client   *TestClient
	patterns []string
}

var _ = check.Suite(&IntegrationSuite{})

type J map[string]interface{}

func (i *IntegrationSuite) loadPatterns() error {
	f, e := os.Open(*patterns)
	if e != nil {
		return e
	}
	defer f.Close()
	scan := bufio.NewScanner(f)
	for scan.Scan() {
		if len(scan.Text()) > 0 {
			i.patterns = append(i.patterns, scan.Text())
		}
	}
	if e = scan.Err(); e != nil {
		return e
	}
	return nil
}

func (i *IntegrationSuite) SetUpSuite(c *check.C) {
	if *repo == "" {
		c.Skip("No test.repo specified.")
		return
	}

	var err error

	if err = i.loadPatterns(); err != nil {
		c.Fatal("loading patterns", err)
	}

	i.config, err = ioutil.TempFile("", "livegrep")
	if err != nil {
		c.Fatalf("TempFile: %s", err)
	}

	i.index, err = ioutil.TempFile("", "livegrep")
	if err != nil {
		c.Fatalf("TempFile: %s", err)
	}
	enc := json.NewEncoder(i.config)
	config := J{
		"name": "livegrep",
		"repositories": []J{
			J{
				"name":      "test",
				"path":      *repo,
				"revisions": []string{"HEAD"},
			},
		},
	}
	if err = enc.Encode(config); err != nil {
		c.Fatalf("Encode: %s", err)
	}

	i.client, err = NewClient(
		"--dump_index", i.index.Name(),
		"--max_matches", "100000",
		"--timeout", "100000",
		"--line_limit", strconv.Itoa(LineLimit),
		i.config.Name())
	if err != nil {
		c.Fatalf("NewClient: %s", err)
	}
}

func (i *IntegrationSuite) TearDownSuite(*check.C) {
	if i.client != nil {
		i.client.Close()
	}
	if i.index != nil {
		os.Remove(i.index.Name())
		i.index.Close()
	}
	if i.config != nil {
		os.Remove(i.config.Name())
		i.config.Close()
	}
}

func (i *IntegrationSuite) TestBasic(c *check.C) {
	search, err := i.client.Search(
		context.Background(),
		&pb.Query{Line: "."})
	c.Check(search, check.Not(check.IsNil))
	c.Check(err, check.IsNil)
}

type Match struct {
	Path string
	Line int
}

type SortMatches []Match

func ltMatch(l, r Match) bool {
	if l.Path != r.Path {
		return l.Path < r.Path
	}
	return l.Line < r.Line
}

func (s SortMatches) Less(i, j int) bool {
	return ltMatch(s[i], s[j])
}
func (s SortMatches) Len() int {
	return len(s)
}

func (s SortMatches) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

var grepRE = regexp.MustCompile(`^HEAD:([^:]+):(\d+):`)

func readLineDropTooLong(r *bufio.Reader) ([]byte, error) {
	for {
		line, isPrefix, err := r.ReadLine()
		if err != nil {
			return nil, err
		}
		if !isPrefix {
			line = bytes.TrimRight(line, "\n")
			return line, nil
		}
		for isPrefix {
			line, isPrefix, err = r.ReadLine()
			if err != nil {
				return nil, err
			}
		}
	}
}

func gitGrep(path, regex string, casefold bool) ([]Match, error) {
	args := []string{"grep", "-n", "-I", "-E", "-e", regex}
	if casefold {
		args = append(args, "-i")
	}
	args = append(args, "HEAD")

	cmd := exec.Command("git", args...)
	cmd.Dir = path
	out, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}

	var matches []Match

	cmd.Start()
	buf := bufio.NewReader(out)
	for {
		line, err := readLineDropTooLong(buf)
		if err != nil {
			if err == io.EOF {
				break
			}
			cmd.Process.Kill()
			return nil, err
		}
		m := grepRE.FindSubmatch(line)
		if m == nil {
			cmd.Process.Kill()
			return nil, fmt.Errorf("unparsable: `%s'", line)
		}
		if len(line)-len(m[0]) > LineLimit {
			continue
		}
		lno, _ := strconv.Atoi(string(m[2]))
		matches = append(matches, Match{string(m[1]), lno})
	}
	cmd.Wait()

	return matches, nil
}

func cmpMatches(c *check.C, lhs []Match, rhs []Match) {
	var i, j int
	ok := true
	for i < len(lhs) && j < len(rhs) {
		switch {
		case i == len(lhs):
			c.Log("+", rhs[j])
			ok = false
			j++
		case j == len(rhs):
			c.Log("-", lhs[i])
			ok = false
			i++
		case reflect.DeepEqual(lhs[i], rhs[j]):
			i++
			j++
		case ltMatch(lhs[i], rhs[j]):
			c.Log("-", lhs[i])
			ok = false
			i++
		case ltMatch(rhs[j], lhs[i]):
			c.Log("+", rhs[j])
			ok = false
			j++
		}
	}

	if !ok {
		c.Error("mismatched results")
	}
}

func (i *IntegrationSuite) crosscheck(c *check.C, regex string, casefold bool) {
	c.Logf("crosschecking regex=%q", regex)
	gitMatches, err := gitGrep(*repo, regex, casefold)
	if err != nil {
		c.Fatalf("git grep: %s", err)
	}

	var livegrepMatches []Match
	search, err := i.client.Search(
		context.Background(),
		&pb.Query{Line: regex, FoldCase: casefold})
	if err != nil {
		c.Fatalf("Query: %s", err)
	}
	for _, m := range search.Results {
		livegrepMatches = append(livegrepMatches, Match{m.Path, int(m.LineNumber)})
	}

	sort.Sort(SortMatches(gitMatches))
	sort.Sort(SortMatches(livegrepMatches))

	cmpMatches(c, gitMatches, livegrepMatches)
}

func (i *IntegrationSuite) TestCrosscheck(c *check.C) {
	for _, p := range i.patterns {
		i.crosscheck(c, p, true)
		i.crosscheck(c, p, false)
	}
}
