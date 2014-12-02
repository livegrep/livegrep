package test

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"regexp"
	"sort"
	"strconv"
	"testing"

	"github.com/livegrep/livegrep/client"
	"gopkg.in/check.v1"
)

func Test(t *testing.T) { check.TestingT(t) }

var repo = flag.String("test.repo", "", "Git repository to run integration tests against.")

type IntegrationSuite struct {
	config *os.File
	index  *os.File
	client client.Client
}

var _ = check.Suite(&IntegrationSuite{})

type J map[string]interface{}

func (i *IntegrationSuite) SetUpSuite(c *check.C) {
	if *repo == "" {
		c.Skip("No test.root specified.")
		return
	}

	var err error

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
	search, err := i.client.Query(&client.Query{Line: "."})
	if err != nil {
		c.Fatalf("Query: %s", err)
	}
	for _ = range search.Results() {
	}
	stats, err := search.Close()
	c.Check(stats, check.Not(check.IsNil))
	c.Check(err, check.IsNil)
}

type Match struct {
	Path string
	Line int
}

type SortMatches []Match

func (s SortMatches) Less(i, j int) bool {
	l, r := s[i], s[j]
	if l.Path != r.Path {
		return l.Path < r.Path
	}
	return l.Line < r.Line
}
func (s SortMatches) Len() int {
	return len(s)
}

func (s SortMatches) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

var grepRE = regexp.MustCompile(`^HEAD:([^:]+):(\d+):`)

func gitGrep(path, regex string) ([]Match, error) {
	cmd := exec.Command("git", "grep", "-n", "-E", "-e", regex, "HEAD")
	cmd.Dir = path
	out, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}

	var matches []Match

	cmd.Start()
	scan := bufio.NewScanner(out)
	for scan.Scan() {
		line := scan.Bytes()
		m := grepRE.FindSubmatch(line)
		if m == nil {
			return nil, fmt.Errorf("unparsable: `%s'", line)
		}
		lno, _ := strconv.Atoi(string(m[2]))
		matches = append(matches, Match{string(m[1]), lno})
	}
	cmd.Wait()

	return matches, nil
}

func (i *IntegrationSuite) crosscheck(c *check.C, regex string) {
	gitMatches, err := gitGrep(*repo, regex)
	if err != nil {
		c.Fatalf("git grep: %s", err)
	}

	var livegrepMatches []Match
	search, err := i.client.Query(&client.Query{Line: regex})
	if err != nil {
		c.Fatalf("Query: %s", err)
	}
	for m := range search.Results() {
		livegrepMatches = append(livegrepMatches, Match{m.Path, m.LineNumber})
	}
	search.Close()

	sort.Sort(SortMatches(gitMatches))
	sort.Sort(SortMatches(livegrepMatches))

	c.Check(livegrepMatches, check.DeepEquals, gitMatches)
}

func (i *IntegrationSuite) TestCrosscheck(c *check.C) {
	i.crosscheck(c, `hello`)
}
