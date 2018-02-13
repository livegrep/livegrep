package blameworthy

import (
	"fmt"
	"os"
	"strings"
	"testing"
)

func TestLogParsing(t *testing.T) {
	test_log_parsing_file(t, "test_data/git-log.dashing")
	test_log_parsing_file(t, "test_data/git-log.dashing.stripped")
}

func test_log_parsing_file(t *testing.T, path string) {
	file, err := os.Open(path)
	if err != nil {
		t.Error(err)
	}
	defer file.Close()
	history, err := ParseGitLog(file)
	a := []string{}
	for k, diffs := range history.Files {
		a = append(a, fmt.Sprint(k, " -> "))
		for _, d := range diffs {
			a = append(a, fmt.Sprintf("{%v %v %v}",
				d.Commit.Hash, d.Path, d.Hunks))
		}
	}
	actual := strings.Join(a, "")
	wanted := "test.txt -> " +
		"{b9a26a4383eb51c1 test.txt [{0 0 1 3}]}" +
		"{b0539826eadc3feb test.txt [{1 2 1 2}]}" +
		"{42838bca4ba13c3f test.txt [{1 3 0 0}]}"
	if actual != wanted {
		t.Fatalf(
			"Git log parsed incorrectly\nWanted: %v\nActual: %v",
			wanted, actual,
		)
	}
}
