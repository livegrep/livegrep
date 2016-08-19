package libhoney

import (
	"fmt"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"
	"time"
)

func testOK(t testing.TB, err error) {
	if err != nil {
		_, file, line, _ := runtime.Caller(1)
		t.Fatalf("%s:%d: unexpected error: %s", filepath.Base(file), line, err.Error())
	}
}
func testErr(t testing.TB, err error) {
	if err == nil {
		_, file, line, _ := runtime.Caller(1)
		t.Fatalf("%s:%d: error expected!", filepath.Base(file), line)
	}
}

func testEquals(t testing.TB, actual, expected interface{}, msg ...string) {
	if !reflect.DeepEqual(actual, expected) {
		testCommonErr(t, actual, expected, msg)
	}
}

func testNotEquals(t testing.TB, actual, expected interface{}, msg ...string) {
	if reflect.DeepEqual(actual, expected) {
		testCommonErr(t, actual, expected, msg)
	}
}

func testResponsesChannelEmpty(t testing.TB, chnl chan Response, msg ...string) {
	expected := "no response"
	var rsp string
	select {
	case _ = <-chnl:
		rsp = "got response"
	default:
		rsp = "no response"
	}
	testEquals(t, rsp, expected)
}

func testChannelHasResponse(t testing.TB, chnl chan Response, msg ...string) {
	expected := "got response"
	var r Response
	var rsp string
	select {
	case r = <-chnl:
		rsp = "got response"
	default:
		rsp = "no response"
	}
	t.Log(r)
	testEquals(t, rsp, expected)
}

func testCommonErr(t testing.TB, actual, expected interface{}, msg []string) {
	message := strings.Join(msg, ", ")
	_, file, line, _ := runtime.Caller(2)

	t.Errorf(
		"%s:%d: %s -- actual(%T): %v, expected(%T): %v",
		filepath.Base(file),
		line,
		message,
		testDeref(actual),
		testDeref(actual),
		testDeref(expected),
		testDeref(expected),
	)
}

func testDeref(v interface{}) interface{} {
	switch t := v.(type) {
	case *string:
		return fmt.Sprintf("*(%v)", *t)
	case *int64:
		return fmt.Sprintf("*(%v)", *t)
	case *float64:
		return fmt.Sprintf("*(%v)", *t)
	case *bool:
		return fmt.Sprintf("*(%v)", *t)
	default:
		return v
	}
}

// for easy time manipulation during tests
type fakeNower struct {
	now  time.Time
	iter int
}

// Now() returns the nth element of nows
func (f *fakeNower) Now() time.Time {
	now := f.now.Add(time.Second * 10 * time.Duration(f.iter))
	f.iter += 1
	return now
}
func (f *fakeNower) set(t time.Time) {
	f.now = t
}
func (f *fakeNower) init() {
	f.iter = 0
	t0, _ := time.Parse(time.RFC3339, "2010-06-21T15:04:05Z")
	f.now = t0
}
