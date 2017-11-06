package main

import (
	"context"
	"flag"
	"log"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
	"google.golang.org/grpc"
)

var (
	flagReloadBackend = flag.String("backend", "", "Backend to reload")
)

func main() {
	flag.Parse()
	log.SetFlags(0)

	if len(flag.Args()) != 0 {
		log.Fatal("Expected no arguments")
	}

	if err := reloadBackend(*flagReloadBackend); err != nil {
		log.Fatalln("reload:", err.Error())
	}
}


func reloadBackend(addr string) error {
	client, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		return err
	}

	codesearch := pb.NewCodeSearchClient(client)

	if _, err = codesearch.Reload(context.Background(), &pb.Empty{}, grpc.FailFast(false)); err != nil {
		return err
	}
	return nil
}
