package test

import (
	"flag"
	"testing"

	"github.com/nelhage/livegrep/client"
)

var index = flag.String("index", "", "Path to an index to run benchmarks against")

func benchmarkQuery(b *testing.B, q *client.Query) {
	if *index == "" {
		b.SkipNow()
	}

	c, e := NewClient("-load_index", *index)
	if e != nil {
		b.Fatal(e.Error())
	}

	for i := 0; i < b.N+1; i++ {
		if i == 1 {
			// Don't count the first run+setup, to make
			// sure everything is primed.
			b.ResetTimer()
		}
		if e := c.Err(); e != nil {
			b.Fatalf("err: %s", e.Error())
		}
		s, e := c.Query(q)
		if e != nil {
			b.Fatalf("query: %s", e.Error())
		}
		for _ = range s.Results() {
		}
		if _, e := s.Close(); e != nil {
			b.Fatalf("close: %s", e.Error())
		}
	}
}

func BenchmarkUUID(b *testing.B) {
	benchmarkQuery(b, &client.Query{
		Line: `[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}`,
	})
}

func BenchmarkAlphanum20(b *testing.B) {
	benchmarkQuery(b, &client.Query{Line: `[0-9a-f]{20}`})
}

func BenchmarkAlphanum50(b *testing.B) {
	benchmarkQuery(b, &client.Query{Line: `[0-9a-f]{50}`})
}

func BenchmarkAlphanum100(b *testing.B) {
	benchmarkQuery(b, &client.Query{Line: `[0-9a-f]{50}`})
}
