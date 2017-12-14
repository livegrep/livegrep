package main

import (
	"context"
	"flag"
	"log"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
	"google.golang.org/grpc"
)

func main() {
	flag.Parse()
	log.SetFlags(0)

	if len(flag.Args()) != 1 {
		log.Fatal("You must provide a HOST:PORT to reload")
	}

	if err := reloadBackend(flag.Arg(0)); err != nil {
		log.Fatalln("reload:", err.Error())
	}
}

func reloadBackend(addr string) error {
	client, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		return err
	}

	codesearch := pb.NewCodeSearchClient(client)

	if _, err = codesearch.Reload(context.Background(), &pb.Empty{}, grpc.FailFast(true)); err != nil {
		return err
	}
	return nil
}
