// Package config provides support for loading config options from a
// file, leveraging configuration defined using the `flag' package.
//
// Config files should consist of lines of the form
//
//   <key> = <value>
//
// or
//
//   # this is a comment
//
// Configuration options are loaded by passing in a flag.FlagSet; keys
//  and values are looked up, parsed, and stored using the
//  FlagSet. For many applications, you can just pass in
//  flag.CommandLine to expose your application's default set of
//  command-line options as configuration frobs.
package config

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"os"
	"strings"
)

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
