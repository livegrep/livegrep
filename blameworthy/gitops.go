package blameworthy

import (
	"bufio"
	"fmt"
	"io"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
)

const HashLength = 16 // number of hash characters to preserve

type GitHistory struct {
	Hashes  []string
	Commits map[string]*Commit
	Files   map[string]File
}

type Commit struct {
	Hash   string
	Author string
	Date   int32 // YYYYMMDD
	Diffs  []*Diff
}

type File []Diff

type Diff struct {
	Commit *Commit
	Path   string
	Hunks  []Hunk
}

type Hunk struct {
	OldStart  int
	OldLength int
	NewStart  int
	NewLength int
}

func RunGitLog(repository_path string, revision string) (io.ReadCloser, error) {
	cmd := exec.Command("git",
		"-C", repository_path,
		"log",
		"-U0",
		"--format=commit %H%nAuthor: %ae%nDate: %cd",
		"--date=format:%Y%m%d",
		"--no-prefix",
		"--no-renames",
		"--reverse",

		// Avoid invoking custom diff commands or conversions.
		"--no-ext-diff",
		"--no-textconv",

		// Treat a merge as a simple diff against its 1st parent:
		"--first-parent",
		"-m",

		revision,
	)
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}
	//defer cmd.Wait()  // drat, when will we do this?
	err = cmd.Start()
	if err != nil {
		return nil, err
	}
	return stdout, nil
}

// Given an input stream from `git log`, print out an abbreviated form
// of the log that is missing the "+" and "-" lines that give the actual
// content of each diff.  Each line like "@@ -0,0 +1,3 @@" introducing
// content will have its final double-at suffixed with a dash (like
// this: "@@-") so blameworthy will recognize that the content has been
// omitted when it reads the log as input.
func StripGitLog(input io.Reader) error {
	re, _ := regexp.Compile(`^@@ -(\d+),?(\d*) \+(\d+),?(\d*) `)

	scanner := bufio.NewScanner(input)

	const maxCapacity = 100 * 1024 * 1024
	buf := make([]byte, maxCapacity)
	scanner.Buffer(buf, maxCapacity)

	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "commit ") {

		} else if strings.HasPrefix(line, "Author: ") {

		} else if strings.HasPrefix(line, "Date: ") {

		} else if strings.HasPrefix(line, "--- ") {

		} else if strings.HasPrefix(line, "+++ ") {

		} else if strings.HasPrefix(line, "@@ ") {
			rest := line[3:]
			i := strings.Index(rest, " @@")
			line = fmt.Sprintf("@@ %s @@-", rest[:i])

			result_slice := re.FindStringSubmatch(line)
			oldLength := 1
			if len(result_slice[2]) > 0 {
				oldLength, _ = strconv.Atoi(result_slice[2])
			}
			newLength := 1
			if len(result_slice[4]) > 0 {
				newLength, _ = strconv.Atoi(result_slice[4])
			}
			lines_to_skip := oldLength + newLength
			for i := 0; i < lines_to_skip; i++ {
				scanner.Scan()
			}
		} else {
			continue
		}
		_, err := fmt.Print(line, "\n")
		if err != nil {
			return nil // be silent about error if piped into head, tail
		}
	}
	return scanner.Err()
}

func ParseGitLog(input_stream io.ReadCloser) (*GitHistory, error) {
	scanner := bufio.NewScanner(input_stream)

	// Give the scanner permission to read very long lines, to
	// prevent it from exiting with an error on the first compressed
	// js file it encounters.
	buf := make([]byte, 64*1024)
	scanner.Buffer(buf, 1024*1024*1024)

	history := GitHistory{}
	history.Commits = make(map[string]*Commit)
	history.Files = make(map[string]File)

	commits := history.Commits
	files := history.Files

	authors := map[string]string{} // dedup authors

	var hash string
	var commit *Commit
	var diff *Diff

	// A dash after the second "@@" is a signal from our command
	// `strip-git-log` that it has removed the "+" and "-" lines
	// that would have followed next.
	re, _ := regexp.Compile(`@@ -(\d+),?(\d*) \+(\d+),?(\d*) @@(-?)`)

	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "commit ") {
			hash = line[7 : 7+HashLength]
			history.Hashes = append(history.Hashes, hash)
			commit = &Commit{hash, "", 0, nil}
			commits[hash] = commit
		} else if strings.HasPrefix(line, "--- ") {
			path := line[4:]
			scanner.Scan() // read the "+++" line
			if path == "/dev/null" {
				line2 := scanner.Text()
				path = line2[4:]
			}
			files[path] = append(files[path],
				Diff{commit, path, []Hunk{}})
			diff = &files[path][len(files[path])-1]
			commit.Diffs = append(commit.Diffs, diff)
		} else if strings.HasPrefix(line, "@@ ") {
			result_slice := re.FindStringSubmatch(line)
			OldStart, _ := strconv.Atoi(result_slice[1])
			OldLength := 1
			if len(result_slice[2]) > 0 {
				OldLength, _ = strconv.Atoi(result_slice[2])
			}
			NewStart, _ := strconv.Atoi(result_slice[3])
			NewLength := 1
			if len(result_slice[4]) > 0 {
				NewLength, _ = strconv.Atoi(result_slice[4])
			}

			diff.Hunks = append(diff.Hunks,
				Hunk{OldStart, OldLength, NewStart, NewLength})

			// Expect no unified diff if hunk header ends in "@@-"
			is_stripped := len(result_slice[5]) > 0
			if !is_stripped {
				lines_to_skip := OldLength + NewLength
				for i := 0; i < lines_to_skip; i++ {
					scanner.Scan()
				}
			}
		} else if len(commit.Author) == 0 && strings.HasPrefix(line, "Author: ") {
			a := strings.TrimSpace(line[8:])
			a2, ok := authors[a]
			if ok {
				commit.Author = a2
			} else {
				authors[a] = a
			}
		} else if commit.Date == 0 && strings.HasPrefix(line, "Date: ") {
			// TODO: also learn to parse normal "git log" dates?
			n, _ := strconv.Atoi(line[6:])
			commit.Date = int32(n)
		}
	}
	return &history, scanner.Err()
}
