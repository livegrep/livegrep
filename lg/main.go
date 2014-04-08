package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"github.com/nelhage/livegrep/server/api"
	"net/http"
	"net/url"
	"os"
)

var (
	server = flag.String("server", "http://localhost:8910", "The livegrep server to connect to")
)

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [flags] REGEX", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()

	if len(flag.Args()) != 1 {
		flag.Usage()
		os.Exit(1)
	}

	uri, err := url.Parse(*server)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Parsing server %s: %s\n", *server, err.Error())
		os.Exit(1)
	}

	uri.Path = "/api/v1/search/"
	uri.RawQuery = url.Values{"line": []string{flag.Arg(0)}}.Encode()

	resp, err := http.Get(uri.String())

	if err != nil {
		fmt.Fprintf(os.Stderr, "Requesting %s: %s\n", uri.String(), err.Error())
		os.Exit(1)
	}

	if resp.StatusCode != 200 {
		var reply api.ReplyError
		if e := json.NewDecoder(resp.Body).Decode(&reply); e != nil {
			fmt.Fprintf(os.Stderr,
				"Error reading reply (status=%d): %s\n", resp.StatusCode, e.Error())
		} else {
			fmt.Fprintf(os.Stderr, "Error: %s: %s", reply.Err.Code, reply.Err.Message)
		}
		os.Exit(1)
	}

	var reply api.ReplySearch

	if e := json.NewDecoder(resp.Body).Decode(&reply); e != nil {
		fmt.Fprintf(os.Stderr,
			"Error reading reply (status=%d): %s\n", resp.StatusCode, e.Error())
		os.Exit(1)
	}

	for _, r := range reply.Results {
		ctx := r.Contexts[0]
		p := ctx.Paths[0]
		if p.Repo != "" {
			fmt.Printf("%s:", p.Repo)
		}
		fmt.Printf("%s:%s:%d: ", p.Ref, p.Path, ctx.LineNumber)
		fmt.Printf("%s\n", r.Line)
	}
}
