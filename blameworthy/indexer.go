package blameworthy

import (
	"fmt"
)

type BlameSegment struct {
	LineCount  int
	LineStart  int
	CommitHash string
}

type BlameSegments []BlameSegment

type BlameLine struct {
	CommitHash string
	LineNumber int
}

type BlameVector []BlameLine

type BlameResult struct {
	BlameVector        BlameVector
	FutureVector       BlameVector
	PreviousCommitHash string
	NextCommitHash     string
	Hunks              []Hunk
}

func (history GitHistory) DiffBlame(commitHash string, path string) (*BlameResult, error) {
	commits, ok := history.Files[path]
	if !ok {
		return nil, fmt.Errorf("no such file: %v", path)
	}
	i := 0
	for i = range commits {
		if commits[i].Hash == commitHash {
			break
		}
	}
	if i == len(commits) {
		return nil, fmt.Errorf("commit %v did not change file %v",
			commitHash, path)
	}
	r := BlameResult{}
	r.Hunks = commits[i].Hunks
	r.BlameVector, r.FutureVector = blame(commits, i+1, -1)
	if i-1 >= 0 {
		r.PreviousCommitHash = commits[i-1].Hash
	}
	if i+1 < len(commits) {
		r.NextCommitHash = commits[i+1].Hash
	}
	return &r, nil
}

func (history GitHistory) FileBlame(commitHash string, path string) (*BlameResult, error) {
	fileHistory, i, err := history.findCommit(commitHash, path)
	if err != nil {
		return nil, err
	}
	i-- // TODO: inline findCommit so we don't need this
	r := BlameResult{}
	r.BlameVector, r.FutureVector = blame(fileHistory, i+1, 0)
	if fileHistory[i].Hash == commitHash {
		r.PreviousCommitHash = getHash(fileHistory, i-1)
		r.NextCommitHash = getHash(fileHistory, i+1)
	} else {
		r.PreviousCommitHash = getHash(fileHistory, i)
		r.NextCommitHash = getHash(fileHistory, i+1)
	}
	return &r, nil
}

func (history GitHistory) findCommit(commitHash string, path string) ([]Diff, int, error) {
	fileHistory, ok := history.Files[path]
	if !ok {
		return []Diff{}, -1, fmt.Errorf("no such file: %v", path)
	}
	i := 0
	j := 0
	for ; i < len(history.Hashes); i++ {
		h := history.Hashes[i]
		if j < len(fileHistory) && fileHistory[j].Hash == h {
			j++
		}
		if h == commitHash {
			break
		}
	}
	if i == len(history.Hashes) {
		return []Diff{}, -1, fmt.Errorf("no such commit: %v", commitHash)
	}
	if j == 0 {
		return []Diff{}, -1, fmt.Errorf("file %s does not exist at commit %s",
			path, commitHash)
	}
	return fileHistory, j, nil
}

func blame(history []Diff, end int, bump int) (BlameVector, BlameVector) {
	segments := BlameSegments{}
	var i int
	for i = 0; i < end+bump; i++ {
		commit := history[i]
		segments = commit.step(segments)
	}
	blameVector := segments.flatten()
	for ; i < len(history); i++ {
		commit := history[i]
		segments = commit.step(segments)
	}
	segments = segments.wipe()
	reverse_in_place(history)
	for i--; i > end-1; i-- {
		commit := history[i]
		segments = commit.step(segments)
	}
	reverse_in_place(history)
	futureVector := segments.flatten()
	return blameVector, futureVector
}

// Return the hash of the i'th array member if i is in-bounds, else "".
// This makes the above code slightly less verbose.
func getHash(history []Diff, i int) string {
	if i >= 0 && i < len(history) {
		return history[i].Hash
	}
	return ""
}

func (commit Diff) step(oldb BlameSegments) BlameSegments {
	newb := BlameSegments{}
	olineno := 1
	nlineno := 1

	oi := 0
	ocount := 0
	if len(oldb) > 0 {
		ocount = oldb[0].LineCount
	}

	ff := func(linecount int) {
		// fmt.Print("ff ", linecount, "\n")
		for linecount > 0 && linecount >= ocount {
			// fmt.Print(linecount, oldb, oi, "\n")
			progress := oldb[oi].LineCount - ocount
			start := oldb[oi].LineStart + progress
			hash := oldb[oi].CommitHash
			newb = append(newb, BlameSegment{ocount, start, hash})
			nlineno += ocount
			linecount -= ocount
			olineno += ocount
			oi += 1
			ocount = 0
			if oi < len(oldb) {
				ocount = oldb[oi].LineCount
			}
		}
		if linecount > 0 {
			progress := oldb[oi].LineCount - ocount
			start := oldb[oi].LineStart + progress
			commit_hash := oldb[oi].CommitHash
			newb = append(newb,
				BlameSegment{linecount, start, commit_hash})
			nlineno += linecount
			ocount -= linecount
			olineno += linecount
		}
	}
	skip := func(linecount int) {
		// fmt.Print("skip ", linecount, ocount, oi, oldb, "\n")
		for linecount > 0 && linecount >= ocount {
			linecount -= ocount
			olineno += ocount
			oi += 1
			ocount = 0
			if oi < len(oldb) {
				ocount = oldb[oi].LineCount
			}
		}
		ocount -= linecount
		olineno += linecount
		// olineno += linecount
		// fmt.Print("skip done")
	}
	add := func(linecount int, commit_hash string) {
		// fmt.Print("add ", linecount, commit_hash, "\n")
		start := nlineno
		newb = append(newb, BlameSegment{linecount, start, commit_hash})
		nlineno += linecount
	}

	for _, h := range commit.Hunks {
		// fmt.Print("HUNK ", h, "\n")
		if h.OldLength > 0 {
			ff(h.OldStart - olineno)
			skip(h.OldLength)
		}
		if h.NewLength > 0 {
			ff(h.NewStart - nlineno)
			add(h.NewLength, commit.Hash)
		}
	}

	for oi < len(oldb) {
		// fmt.Print("Trying to ff", ocount, "\n")
		if ocount > 0 {
			ff(ocount)
		} else {
			oi += 1
			ocount = 0
			if oi < len(oldb) {
				ocount = oldb[oi].LineCount
			}
		}
	}

	return newb
}

func reverse_in_place(commits []Diff) {
	// Reverse the effect of each hunk.
	for i := range commits {
		for j := range commits[i].Hunks {
			h := &commits[i].Hunks[j]
			h.OldStart, h.NewStart = h.NewStart, h.OldStart
			h.OldLength, h.NewLength = h.NewLength, h.OldLength
		}
	}
}

func (segments BlameSegments) wipe() BlameSegments {
	n := 0
	for _, segment := range segments {
		n += segment.LineCount
	}
	return BlameSegments{{n, 1, ""}}
}

func (segments BlameSegments) flatten() BlameVector {
	v := BlameVector{}
	for _, segment := range segments {
		for i := 0; i < segment.LineCount; i++ {
			n := segment.LineStart + i
			v = append(v, BlameLine{segment.CommitHash, n})
		}
	}
	return v
}
