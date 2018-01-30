package server

import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/livegrep/livegrep/blameworthy"
	"github.com/livegrep/livegrep/server/config"
)

// Blame experiment.

type BlameData struct {
	CommitHash     string
	PreviousCommit string
	NextCommit     string
	Author         string
	Date           string
	Subject        string
	Lines          []BlameLine
	Content        string
}

type DiffData struct {
	CommitHash     string
	PreviousCommit string
	NextCommit     string
	Author         string
	Date           string
	Subject        string
	FileDiffs      []DiffFileData
}

type DiffFileData struct {
	Path    string
	Lines   []BlameLine
	Content string
}

type BlameLine struct {
	PreviousCommit     string
	PreviousLineNumber int
	NextCommit         string
	NextLineNumber     int
	OldLineNumber      int
	NewLineNumber      int
	Symbol             string
}

const blankHash = "                " // as wide as a displayed hash
var histories = make(map[string]*blameworthy.GitHistory)

func initBlame(cfg *config.Config) error {
	for _, r := range cfg.IndexConfig.Repositories {
		blame, ok := r.Metadata["blame"]
		if !ok {
			continue
		}
		var gitLogOutput io.ReadCloser
		if blame == "git" {
			var err error
			gitLogOutput, err = blameworthy.RunGitLog(r.Path, "HEAD")
			if err != nil {
				return err
			}
		} else {
			var err error
			gitLogOutput, err = os.Open(blame)
			if err != nil {
				return err
			}
		}
		gitHistory, err := blameworthy.ParseGitLog(gitLogOutput)
		if err != nil {
			return err
		}
		histories[r.Name] = gitHistory
	}
	return nil
}

func resolveCommit(repo config.RepoConfig, commitName string, data *BlameData) error {
	output, err := gitShowCommit(commitName, repo.Path)
	if err != nil {
		return err
	}
	lines := strings.Split(output, "\n")
	data.CommitHash = lines[0][:blameworthy.HashLength]
	data.Author = lines[1]
	data.Date = lines[2]
	data.Subject = lines[3]
	return nil
}

func buildBlameData(
	repo config.RepoConfig,
	commitHash string,
	gitHistory *blameworthy.GitHistory,
	path string,
	isDiff bool,
	data *BlameData,
) error {
	start := time.Now()

	obj := commitHash + ":" + path
	content, err := gitCatBlob(obj, repo.Path)
	if err != nil {
		return err
	}

	lines := []BlameLine{}
	var result *blameworthy.BlameResult

	// Easy enough: simply enumerate the lines of the file.

	result, err = gitHistory.FileBlame(commitHash, path)
	if err != nil {
		return err
	}

	for i, b := range result.BlameVector {
		f := result.FutureVector[i]
		lines = append(lines, BlameLine{
			orBlank(b.CommitHash),
			b.LineNumber,
			orStillExists(f.CommitHash),
			f.LineNumber,
			i + 1,
			0,
			"",
		})
	}

	elapsed := time.Since(start)
	log.Print("Whole thing took ", elapsed)

	data.PreviousCommit = result.PreviousCommitHash
	data.NextCommit = result.NextCommitHash
	data.Lines = lines
	data.Content = content
	return nil
}

func fileRedirect(gitHistory *blameworthy.GitHistory, repoName, hash, path, dest string) (string, error) {
	j := strings.Index(dest, ".")
	if j == -1 {
		// Redirect to this same file but at another commit.
		url := fmt.Sprint("/blame/", repoName, "/", dest, "/", path, "/")
		return url, nil
	}
	// Otherwise, redirect to a specific file and line in a diff.
	destHash := dest[:j]
	fragment := dest[j+1:]

	var k int
	var diff *blameworthy.Diff
	for k, diff = range gitHistory.Commits[destHash] {
		if diff.Path == path {
			break
		}
	}

	url := fmt.Sprint("/diff/", repoName, "/", destHash, "/#", k, fragment)
	return url, nil
}

func diffRedirect(w http.ResponseWriter, r *http.Request, repoName string, hash string, rest string) {
	gitHistory, ok := histories[repoName]
	if !ok {
		http.Error(w, "Repo not configured for blame", 404)
		return
	}
	i := strings.Index(rest, ".")
	if i == -1 {
		url := fmt.Sprint("/diff/", repoName, "/", rest, "/")
		http.Redirect(w, r, url, 307)
		return
	}
	destHash := rest[:i]
	rest = rest[i+1:]
	var j int
	for j = range rest {
		if rest[j] >= 65 {
			break
		}
	}
	if j == len(rest) {
		http.Error(w, "Not found", 404)
		return
	}
	fmt.Print("A\n", rest, " ", rest[:j], "\n")
	commitIndex, err := strconv.Atoi(rest[:j])
	if err != nil {
		http.Error(w, "Not found", 404)
		return
	}
	fmt.Print("A\n")
	path := gitHistory.Commits[hash][commitIndex].Path

	var fragment, url string
	fmt.Print(rest[j], "\n")
	if rest[j] == 102 { // "f"
		fragment = rest[j+1:]
		url = fmt.Sprint("/blame/", repoName, "/", destHash,
			"/", path, "/#", fragment)
	} else {
		//path := gitHistory.Commits[hash][commitIndex]
		destIndex := indexOfFileInCommit(gitHistory, path, destHash)
		fragment = rest[j:]
		// TODO: need to turn path into index into that other diff
		url = fmt.Sprint("/diff/", repoName, "/", destHash,
			"/#", destIndex, fragment)
	}

	fmt.Print(url, "\n")
	http.Redirect(w, r, url, 307)
}

func indexOfFileInCommit(history *blameworthy.GitHistory, path string, hash string) int {
	for k, diff := range history.Commits[hash] {
		if diff.Path == path {
			return k
		}
	}
	return -1
}

func buildDiffData(
	repo config.RepoConfig,
	commitHash string,
	data *DiffData,
) error {
	start := time.Now()

	gitHistory, ok := histories[repo.Name]
	if !ok {
		return fmt.Errorf("Repo not configured for blame")
	}

	for _, diff := range gitHistory.Commits[commitHash] {
		lines, content_lines, err := extendDiff(repo, commitHash, gitHistory, diff.Path)
		if err != nil {
			return err
		}
		data.FileDiffs = append(data.FileDiffs, DiffFileData{
			diff.Path, lines, strings.Join(content_lines, "\n"),
		})
	}

	elapsed := time.Since(start)
	log.Print("Whole thing took ", elapsed)

	// TODO: add map so this lookup is O(1)?
	var i int
	for i = range gitHistory.Hashes {
		if gitHistory.Hashes[i] == commitHash {
			break
		}
	}
	data.PreviousCommit = ""
	data.NextCommit = ""
	if i-1 >= 0 {
		data.PreviousCommit = gitHistory.Hashes[i-1]
	}
	if i+1 < len(gitHistory.Hashes) {
		data.NextCommit = gitHistory.Hashes[i+1]
	}
	return nil
}

func extendDiff(
	repo config.RepoConfig,
	commitHash string,
	gitHistory *blameworthy.GitHistory,
	path string,
) ([]BlameLine, []string, error) {

	lines := []BlameLine{}
	content_lines := []string{}

	result, err := gitHistory.DiffBlame(commitHash, path)
	if err != nil {
		return lines, content_lines, err
	}
	blameVector := result.BlameVector
	futureVector := result.FutureVector

	new_lines := []string{}
	old_lines := []string{}

	if len(futureVector) > 0 {
		obj := commitHash + ":" + path
		content, err := gitCatBlob(obj, repo.Path)
		if err != nil {
			err = fmt.Errorf("Error getting blob: %s", err)
			return lines, content_lines, err
		}
		new_lines = splitLines(content)
	}

	if len(blameVector) > 0 {
		obj := result.PreviousCommitHash + ":" + path
		content, err := gitCatBlob(obj, repo.Path)
		if err != nil {
			err = fmt.Errorf("Error getting blob: %s", err)
			return lines, content_lines, err
		}
		old_lines = splitLines(content)
	}

	j := 0
	k := 0

	both := func() {
		lines = append(lines, BlameLine{
			orBlank(blameVector[j].CommitHash),
			blameVector[j].LineNumber,
			orStillExists(futureVector[k].CommitHash),
			futureVector[k].LineNumber,
			j + 1,
			k + 1,
			"",
		})
		content_lines = append(content_lines, old_lines[j])
		j++
		k++

	}
	left := func() {
		lines = append(lines, BlameLine{
			orBlank(blameVector[j].CommitHash),
			blameVector[j].LineNumber,
			//"  (this commit) ",
			blankHash,
			0,
			j + 1,
			0,
			"-",
		})
		content_lines = append(content_lines, old_lines[j])
		j++
	}
	right := func() {
		lines = append(lines, BlameLine{
			//"  (this commit) ",
			blankHash,
			0,
			orStillExists(futureVector[k].CommitHash),
			futureVector[k].LineNumber,
			0,
			k + 1,
			"+",
		})
		content_lines = append(content_lines, new_lines[k])
		k++
	}
	context := func(distance int) {
		// fmt.Print("DISTANCE ", distance, " ",
		// 	til_line, " ", j+1, "\n")
		if distance > 9 {
			for i := 0; i < 3; i++ {
				both()
				distance--
			}
			for ; distance > 3; distance-- {
				j++
				k++
			}
			for i := 0; i < 3; i++ {
				lines = append(lines, BlameLine{
					"        .       ",
					0,
					"        .       ",
					//blankHash,
					0,
					0,
					0,
					"",
				})
				content_lines = append(content_lines, "")
			}
		}
		for ; distance > 0; distance-- {
			both()
		}
	}

	for _, h := range result.Hunks {
		if h.OldLength > 0 {
			context(h.OldStart - (j + 1))
			for m := 0; m < h.OldLength; m++ {
				left()
			}
		}
		if h.NewLength > 0 {
			context(h.NewStart - (k + 1))
			for m := 0; m < h.NewLength; m++ {
				right()
			}
		}
	}
	end := len(old_lines) + 1
	context(end - (j + 1))

	content_lines = append(content_lines, "")

	return lines, content_lines, nil
}

func gitShowCommit(commitHash string, repoPath string) (string, error) {
	// git show --pretty="%H%n%an <%ae>%n%ci%n%s" --quiet master master:travisdeps.sh
	out, err := exec.Command(
		"git", "-C", repoPath, "show", "--quiet",
		"--pretty=%H%n%an <%ae>%n%ci%n%s", commitHash,
	).Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func orBlank(s string) string {
	if len(s) > 0 {
		return s
	}
	return blankHash
}

func orStillExists(s string) string {
	if len(s) > 0 {
		return s
	}
	return " (still exists) "
}

func splitLines(s string) []string {
	if len(s) > 0 && s[len(s)-1] == '\n' {
		s = s[:len(s)-1]
	}
	return strings.Split(s, "\n")
}
