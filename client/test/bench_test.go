package test

import (
	"context"
	"flag"
	"testing"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
)

var index = flag.String("index", "", "Path to an index to run benchmarks against")

func benchmarkQuery(b *testing.B, q *pb.Query) {
	if *index == "" {
		b.SkipNow()
	}

	c, e := NewClient("-load_index", *index)
	if e != nil {
		b.Fatal(e.Error())
	}

	b.ResetTimer()
	for i := 0; i < b.N+1; i++ {
		_, e := c.Search(context.Background(), q)
		if e != nil {
			b.Fatalf("query: %s", e.Error())
		}
	}
}

func BenchmarkDazed(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `dazed`})
}

func BenchmarkDazedCaseFold(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `dazed`, FoldCase: true})
}

func BenchmarkDefKmalloc(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `^(\s.*\S)?kmalloc\s*\(`})
}

func BenchmarkSpaceEOL(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `\s$`})
}

func Benchmark10Space(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `\s{10}$`})
}

func BenchmarkUUID(b *testing.B) {
	benchmarkQuery(b, &pb.Query{
		Line: `[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}`,
	})
}

func BenchmarkUUIDCaseFold(b *testing.B) {
	benchmarkQuery(b, &pb.Query{
		Line:     `[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}`,
		FoldCase: true,
	})
}

func BenchmarkAlphanum20(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `[0-9a-f]{20}`})
}

func BenchmarkAlphanum50(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `[0-9a-f]{50}`})
}

func BenchmarkAlphanum100(b *testing.B) {
	benchmarkQuery(b, &pb.Query{Line: `[0-9a-f]{50}`})
}
