package test

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"os"
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

	i.client, err = NewClient("--dump_index", i.index.Name(), i.config.Name())
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
