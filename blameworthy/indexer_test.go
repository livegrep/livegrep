package blameworthy

import (
	"fmt"
	"testing"
)

func TestStepping(t *testing.T) {
	var tests = []struct {
		inputCommits   []Diff
		expectedOutput string
	}{{
		[]Diff{},
		"[]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
		},
		"[[{3 1 a1}]]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 0, 2, 2},
				{2, 0, 5, 2},
			}},
			{"c3", "test.txt", []Hunk{
				{1, 1, 1, 0},
				{4, 2, 3, 1},
			}},
		},
		"[[{3 1 a1}]" +
			" [{1 1 a1} {2 2 b2} {1 2 a1} {2 5 b2} {1 3 a1}]" +
			" [{2 2 b2} {1 3 c3} {1 6 b2} {1 3 a1}]]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 1, 0, 0}, // remove 1st line
				{2, 0, 2, 1}, // add new line 2
			}},
		},
		"[[{3 1 a1}] [{1 2 a1} {1 2 b2} {1 3 a1}]]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 3, 0, 0},
			}},
		},
		"[[{3 1 a1}] []]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{0, 0, 4, 1},
			}},
		},
		"[[{3 1 a1}] [{3 1 a1} {1 4 b2}]]",
	}}
	for testIndex, test := range tests {
		segments := BlameSegments{}
		out := []BlameSegments{}
		for _, commit := range test.inputCommits {
			segments = commit.step(segments)
			out = append(out, segments)
		}
		if fmt.Sprint(out) != test.expectedOutput {
			t.Error("Test", testIndex+1, "failed",
				"\n  Wanted", test.expectedOutput,
				"\n  Got   ", fmt.Sprint(out),
				"\n  From  ", test.inputCommits)
		}
	}
}

func TestAtMethod(t *testing.T) {
	var tests = []struct {
		inputCommits   []Diff
		expectedOutput string
	}{{
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
		}, "" +
			"BLAME [{a1 1} {a1 2} {a1 3}]" +
			"FUTURE [{ 1} { 2} { 3}]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 0, 2, 2},
				{2, 0, 5, 2},
			}},
			{"c3", "test.txt", []Hunk{
				{1, 1, 1, 0},
				{4, 2, 3, 1},
			}},
		}, "" +
			"BLAME [{a1 1} {a1 2} {a1 3}]" +
			"FUTURE [{c3 1} {c3 4} { 5}]" +
			"BLAME [{a1 1} {b2 2} {b2 3} {a1 2} {b2 5} {b2 6} {a1 3}]" +
			"FUTURE [{c3 1} { 1} { 2} {c3 4} {c3 5} { 4} { 5}]" +
			"BLAME [{b2 2} {b2 3} {c3 3} {b2 6} {a1 3}]" +
			"FUTURE [{ 1} { 2} { 3} { 4} { 5}]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 1, 0, 0}, // remove 1st line
				{2, 0, 2, 1}, // add new line 2
			}},
		}, "" +
			"BLAME [{a1 1} {a1 2} {a1 3}]" +
			"FUTURE [{b2 1} { 1} { 3}]" +
			"BLAME [{a1 2} {b2 2} {a1 3}]" +
			"FUTURE [{ 1} { 2} { 3}]",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{1, 3, 0, 0},
			}},
		}, "" +
			"BLAME [{a1 1} {a1 2} {a1 3}]" +
			"FUTURE [{b2 1} {b2 2} {b2 3}]" +
			"BLAME []" +
			"FUTURE []",
	}, {
		[]Diff{
			{"a1", "test.txt", []Hunk{
				{0, 0, 1, 3},
			}},
			{"b2", "test.txt", []Hunk{
				{0, 0, 4, 1},
			}},
		}, "" +
			"BLAME [{a1 1} {a1 2} {a1 3}]" +
			"FUTURE [{ 1} { 2} { 3}]" +
			"BLAME [{a1 1} {a1 2} {a1 3} {b2 4}]" +
			"FUTURE [{ 1} { 2} { 3} { 4}]",
	}}
	for testIndex, test := range tests {
		out := ""

		// Build full GitHistory based on this one lone file history.
		gh := GitHistory{[]string{}, nil, map[string][]Diff{
			"path": test.inputCommits,
		}}
		for _, c := range test.inputCommits {
			gh.Hashes = append(gh.Hashes, c.Hash)
		}

		// Examine the history it produces.
		for _, c := range test.inputCommits {
			commitHash := c.Hash
			r, err := gh.FileBlame(commitHash, "path")
			if err != nil {
				t.Error("Test", testIndex+1, "failed:", err)
				return
			}
			out += fmt.Sprint("BLAME ", r.BlameVector)
			out += fmt.Sprint("FUTURE ", r.FutureVector)
		}
		if fmt.Sprint(out) != test.expectedOutput {
			t.Error("Test", testIndex+1, "failed",
				"\n  Wanted", test.expectedOutput,
				"\n  Got   ", out,
				"\n  From  ", test.inputCommits)
		}
	}
}

func TestPreviousAndNext(t *testing.T) {
	var tests = []struct {
		history         GitHistory
		expectedResults []string
	}{{
		GitHistory{
			[]string{"a1", "b2", "c3", "d4", "e5"},
			nil,
			map[string][]Diff{
				"README": {
					Diff{"b2", "test.txt", []Hunk{{0, 0, 1, 2}}},
					Diff{"d4", "test.txt", []Hunk{{2, 1, 2, 1}}},
				},
			},
		},
		[]string{
			"file README does not exist at commit a1",
			"{[{b2 1} {b2 2}] [{ 1} {d4 2}]  d4 []}",
			"{[{b2 1} {b2 2}] [{ 1} {d4 2}] b2 d4 []}",
			"{[{b2 1} {d4 2}] [{ 1} { 2}] b2  []}",
			"{[{b2 1} {d4 2}] [{ 1} { 2}] d4  []}",
		},
	}}
	for testIndex, test := range tests {
		for i, expectedResult := range test.expectedResults {
			hash := test.history.Hashes[i]
			result, err := test.history.FileBlame(hash, "README")
			output := ""
			if err != nil {
				output = fmt.Sprint(err)
			} else {
				output = fmt.Sprint(*result)
			}
			if output != expectedResult {
				t.Error("Test", testIndex+1,
					"line", i+1,
					"failed",
					"\n  Wanted", expectedResult,
					"\n  Got   ", output)
			}
		}
	}
}
