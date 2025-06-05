package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"strings"

	"github.com/livegrep/livegrep/server/api"
)

var (
	server      = flag.String("server", "http://localhost:8910", "The livegrep server to connect to")
	unixSocket  = flag.String("unix_socket", "", "unix socket path to connect() to as a proxy")
	showVersion = flag.Bool("show_version", false, "Show versions of matched packages")
)

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [flags] REGEX\n", os.Args[0])
		flag.PrintDefaults()
	}
	if err := LoadConfig(flag.CommandLine, "lgrc"); err != nil {
		fmt.Fprintf(os.Stderr, "Loading config: %s\n", err)
		os.Exit(1)
	}
	flag.Parse()

	if len(flag.Args()) == 0 {
		flag.Usage()
		os.Exit(1)
	}

	var uri *url.URL
	var err error

	if strings.Contains(*server, ":") {
		if uri, err = url.Parse(*server); err != nil {
			fmt.Fprintf(os.Stderr, "Parsing server %s: %s\n", *server, err.Error())
			os.Exit(1)
		}
	} else {
		uri = &url.URL{Scheme: "http", Host: *server}
	}

	uri.Path = "/api/v1/search/"
	uri.RawQuery = url.Values{"q": []string{strings.Join(flag.Args(), " ")}}.Encode()

	var transport http.RoundTripper
	if *unixSocket == "" {
		transport = http.DefaultTransport
	} else {
		dialUnix := func(network, addr string) (net.Conn, error) {
			return net.Dial("unix", *unixSocket)
		}
		transport = &http.Transport{
			Dial:              dialUnix,
			DialTLS:           dialUnix,
			DisableKeepAlives: true,
		}
	}
	client := http.Client{Transport: transport}

	resp, err := client.Get(uri.String())

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
			fmt.Fprintf(os.Stderr, "Error: %s: %s\n", reply.Err.Code, reply.Err.Message)
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
		if r.Tree != "" {
			fmt.Printf("%s:", r.Tree)
		}
		if *showVersion && r.Version != "" {
			fmt.Printf("%s:", r.Version)
		}
		fmt.Printf("%s:%d: ", r.Path, r.LineNumber)
		fmt.Printf("%s\n", r.Line)
	}
}

// LoadConfig loads configuration from a dotfile. It looks for
// $HOME/.basename, and, if it exists, opens it and calls
// ParseConfig. Returns silently if no such file exists.
func LoadConfig(flags *flag.FlagSet, basename string) error {
	path := os.ExpandEnv(fmt.Sprintf("${HOME}/.%s", basename))
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	defer f.Close()
	return ParseConfig(flags, f)
}

// ParseConfig parses a config file, using the provided FlagSet to
// look up, parse, and store values.
func ParseConfig(flags *flag.FlagSet, f io.Reader) error {
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		bits := strings.SplitN(line, "=", 2)
		if len(bits) != 2 {
			return fmt.Errorf("illegal config line: `%s'", line)
		}

		key := strings.TrimSpace(bits[0])
		value := strings.TrimSpace(bits[1])

		if flag := flags.Lookup(key); flag == nil {
			return fmt.Errorf("unknown option `%s'", key)
		}

		if err := flags.Set(key, value); err != nil {
			return err
		}
	}
	return nil
}
