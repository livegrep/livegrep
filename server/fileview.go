package server

import (
	"bytes"
	"fmt"
	"net/url"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"sort"
	"strings"

	"github.com/livegrep/livegrep/server/config"
)

// Mapping from known file extensions to filetype hinting.
var filenameToLangMap map[string]string = map[string]string{
	"BUILD":       "python",
	"BUILD.bazel": "python",
	"WORKSPACE":   "python",
}
var extToLangMap map[string]string = map[string]string{
	".adoc":        "AsciiDoc",
	".asc":         "AsciiDoc",
	".asciidoc":    "AsciiDoc",
	".AppleScript": "applescript",
	".bzl":         "python",
	".c":           "c",
	".coffee":      "coffeescript",
	".cpp":         "cpp",
	".css":         "css",
	".go":          "go",
	".h":           "cpp",
	".hs":          "haskell",
	".html":        "markup",
	".java":        "java",
	".js":          "javascript",
	".json":        "json",
	".jsx":         "jsx",
	".m":           "objectivec",
	".markdown":    "markdown",
	".md":          "markdown",
	".mdown":       "markdown",
	".mkdn":        "markdown",
	".mediawiki":   "markdown",
	".nix":         "nix",
	".php":         "php",
	".pl":          "perl",
	".proto":       "go",
	".py":          "python",
	".pyst":        "python",
	".rb":          "ruby",
	".rdoc":        "markdown",
	".rs":          "rust",
	".scala":       "scala",
	".scpt":        "applescript",
	".scss":        "scss",
	".sh":          "bash",
	".sky":         "python",
	".sql":         "sql",
	".swift":       "swift",
	".textile":     "markdown",
	".ts":          "typescript",
	".tsx":         "tsx",
	".wiki":        "markdown",
	".xml":         "markup",
	".yaml":        "yaml",
	".yml":         "yaml",
}

// Grabbed from the extensions GitHub supports here - https://github.com/github/markup
var supportedReadmeExtensions = []string{
	"markdown", "mdown", "mkdn", "md", "textile", "rdoc", "org", "creole", "mediawiki", "wiki",
	"rst", "asciidoc", "adoc", "asc", "pod",
}

var supportedReadmeRegex = buildReadmeRegex(supportedReadmeExtensions)

type breadCrumbEntry struct {
	Name string
	Path string
}

type directoryListEntry struct {
	Name          string
	Path          string
	IsDir         bool
	SymlinkTarget string
}

type fileViewerContext struct {
	PathSegments   []breadCrumbEntry
	Repo           config.RepoConfig
	Commit         string
	DirContent     *directoryContent
	FileContent    *sourceFileContent
	ExternalDomain string
	Permalink      string
	Headlink       string
}

type sourceFileContent struct {
	Content   string
	LineCount int
	Language  string
}

type directoryContent struct {
	Entries       []directoryListEntry
	ReadmeContent *sourceFileContent
}

type DirListingSort []directoryListEntry

func (s DirListingSort) Len() int {
	return len(s)
}

func (s DirListingSort) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

func (s DirListingSort) Less(i, j int) bool {
	if s[i].IsDir != s[j].IsDir {
		return s[i].IsDir
	}
	return s[i].Name < s[j].Name
}

func gitCommitHash(ref string, repoPath string) (string, error) {
	out, err := exec.Command(
		"git", "-C", repoPath, "rev-parse", ref,
	).Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func gitObjectType(obj string, repoPath string) (string, error) {
	out, err := exec.Command("git", "-C", repoPath, "cat-file", "-t", obj).Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(out)), nil
}

func gitCatBlob(obj string, repoPath string) (string, error) {
	out, err := exec.Command("git", "-C", repoPath, "cat-file", "blob", obj).Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}

type gitTreeEntry struct {
	Mode       string
	ObjectType string
	ObjectId   string
	ObjectName string
}

func gitParseTreeEntry(line string) gitTreeEntry {
	dataAndPath := strings.SplitN(line, "\t", 2)
	dataFields := strings.Split(dataAndPath[0], " ")
	return gitTreeEntry{
		Mode:       dataFields[0],
		ObjectType: dataFields[1],
		ObjectId:   dataFields[2],
		ObjectName: dataAndPath[1],
	}
}

func gitListDir(obj string, repoPath string) ([]gitTreeEntry, error) {
	out, err := exec.Command("git", "-C", repoPath, "cat-file", "-p", obj).Output()
	if err != nil {
		return nil, err
	}

	lines := strings.Split(string(out), "\n")
	lines = lines[:len(lines)-1]
	result := make([]gitTreeEntry, len(lines))
	for i, line := range lines {
		result[i] = gitParseTreeEntry(line)
	}
	return result, nil
}

func viewUrl(repo string, path string) string {
	return "/view/" + repo + "/" + path
}

func getFileUrl(repo string, pathFromRoot string, name string, isDir bool) string {
	fileUrl := viewUrl(repo, filepath.Join(pathFromRoot, path.Clean(name)))
	if isDir {
		fileUrl += "/"
	}
	return fileUrl
}

func buildReadmeRegex(supportedReadmeExtensions []string) *regexp.Regexp {
	// Sort in descending order of length so most specific match is selected by regex engine
	sort.Slice(supportedReadmeExtensions, func(i, j int) bool {
		return len(supportedReadmeExtensions[i]) >= len(supportedReadmeExtensions[j])
	})

	// Build regex of form "README.(ext1|ext2)" README case insensitive
	var buf bytes.Buffer
	for i, ext := range supportedReadmeExtensions {
		buf.WriteString(regexp.QuoteMeta(ext))
		if i < len(supportedReadmeExtensions)-1 {
			buf.WriteString("|")
		}
	}
	repoRegexAlt := buf.String()
	repoFileRegex := regexp.MustCompile(fmt.Sprintf("((?i)readme)\\.(%s)", repoRegexAlt))

	return repoFileRegex
}

func buildDirectoryListEntry(treeEntry gitTreeEntry, pathFromRoot string, repo config.RepoConfig) directoryListEntry {
	var fileUrl string
	var symlinkTarget string
	if treeEntry.Mode == "120000" {
		resolvedPath, err := gitCatBlob(treeEntry.ObjectId, repo.Path)
		if err == nil {
			symlinkTarget = resolvedPath
		}
	} else {
		fileUrl = getFileUrl(repo.Name, pathFromRoot, treeEntry.ObjectName, treeEntry.ObjectType == "tree")
	}
	return directoryListEntry{
		Name:          treeEntry.ObjectName,
		Path:          fileUrl,
		IsDir:         treeEntry.ObjectType == "tree",
		SymlinkTarget: symlinkTarget,
	}
}

func buildFileData(relativePath string, repo config.RepoConfig, commit string) (*fileViewerContext, error) {
	commitHash := commit
	out, err := gitCommitHash(commit, repo.Path)
	if err == nil {
		commitHash = out[:strings.Index(out, "\n")]
	}
	cleanPath := path.Clean(relativePath)
	if cleanPath == "." {
		cleanPath = ""
	}
	obj := commitHash + ":" + cleanPath
	pathSplits := strings.Split(cleanPath, "/")

	var fileContent *sourceFileContent
	var dirContent *directoryContent

	objectType, err := gitObjectType(obj, repo.Path)
	if err != nil {
		return nil, err
	}
	if objectType == "tree" {
		treeEntries, err := gitListDir(obj, repo.Path)
		if err != nil {
			return nil, err
		}

		dirEntries := make([]directoryListEntry, len(treeEntries))
		var readmePath, readmeLang string
		for i, treeEntry := range treeEntries {
			dirEntries[i] = buildDirectoryListEntry(treeEntry, cleanPath, repo)
			// Git supports case sensitive files, so README.md & readme.md in the same tree is possible
			// so in this case we just grab the first matching file
			if readmePath != "" {
				continue
			}

			parts := supportedReadmeRegex.FindStringSubmatch(dirEntries[i].Name)
			if len(parts) != 3 {
				continue
			}
			readmePath = obj + parts[0]
			readmeLang = parts[2]
		}

		var readmeContent *sourceFileContent
		if readmePath != "" {
			if content, err := gitCatBlob(readmePath, repo.Path); err == nil {
				readmeContent = &sourceFileContent{
					Content:   content,
					LineCount: strings.Count(content, "\n"),
					Language:  extToLangMap["."+readmeLang],
				}
			}
		}

		sort.Sort(DirListingSort(dirEntries))
		dirContent = &directoryContent{
			Entries:       dirEntries,
			ReadmeContent: readmeContent,
		}
	} else if objectType == "blob" {
		content, err := gitCatBlob(obj, repo.Path)
		if err != nil {
			return nil, err
		}
		language := filenameToLangMap[filepath.Base(cleanPath)]
		if language == "" {
			language = extToLangMap[filepath.Ext(cleanPath)]
		}
		fileContent = &sourceFileContent{
			Content:   content,
			LineCount: strings.Count(string(content), "\n"),
			Language:  language,
		}
	}

	segments := make([]breadCrumbEntry, len(pathSplits))
	for i, name := range pathSplits {
		parentPath := path.Clean(strings.Join(pathSplits[0:i], "/"))
		segments[i] = breadCrumbEntry{
			Name: name,
			Path: getFileUrl(repo.Name, parentPath, name, true),
		}
	}

	externalDomain := "external viewer"
	if url, err := url.Parse(repo.Metadata["url_pattern"]); err == nil {
		externalDomain = url.Hostname()
	}

	permalink := ""
	headlink := ""
	if !strings.HasPrefix(commitHash, commit) {
		permalink = "?commit=" + commitHash[:16]
	} else {
		if dirContent != nil {
			headlink = "."
		} else {
			headlink = segments[len(segments)-1].Name
		}
	}

	return &fileViewerContext{
		PathSegments:   segments,
		Repo:           repo,
		Commit:         commit,
		DirContent:     dirContent,
		FileContent:    fileContent,
		ExternalDomain: externalDomain,
		Permalink:      permalink,
		Headlink:       headlink,
	}, nil
}
