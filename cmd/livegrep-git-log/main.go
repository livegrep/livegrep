/*
This package is a utility that strips diff content from a git log.

This small utility reads a git log from standard input, and writes it to
standard output preserving only four kinds of line:

"commit ..."  <- names the commit
"--- ..."     <- at the top of each file
"+++ ..."     <- at the top of each file
"@@ ..."      <- at the start of each hunk

It edits each "@@ -0,0 +1,3 @@" line so its second "@@" is followed by a
dash instead of a space ("@@-", which never happens in real diffs) so
our blame input routine knows to not expect "+" and "-" lines to follow.
This makes the log far more compact, so livegrep can scan it more
quickly during startup.

*/
package main

import (
	"log"
	"os"

	"github.com/livegrep/livegrep/blameworthy"
)

func main() {
	target := "HEAD"
	if len(os.Args) == 3 {
		target = os.Args[2]
	} else if len(os.Args) != 2 {
		log.Fatalf("usage: %s <repo path> [<revision range>]")
	}
	input, err := blameworthy.RunGitLog(os.Args[1], target)
	if err != nil {
		log.Fatal(err)
	}
	err = blameworthy.StripGitLog(input)
	if err != nil {
		log.Fatal(err)
	}
}
