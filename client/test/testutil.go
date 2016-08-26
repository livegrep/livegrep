package test

import (
	"context"
	"fmt"
	"os"
	"os/exec"

	"google.golang.org/grpc"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
)

type TestClient struct {
	client pb.CodeSearchClient
	cmd    *exec.Cmd
	wait   chan error
}

func (c *TestClient) Info(ctx context.Context, in *pb.InfoRequest, opts ...grpc.CallOption) (*pb.ServerInfo, error) {
	return c.client.Info(ctx, in, opts...)
}

func (c *TestClient) Search(ctx context.Context, in *pb.Query, opts ...grpc.CallOption) (*pb.CodeSearchResult, error) {
	return c.client.Search(ctx, in, opts...)
}

func (c *TestClient) Close() {
	c.cmd.Process.Kill()
	<-c.wait
}

const Codesearch = "../../bazel-bin/src/tools/codesearch"
const Port = 9812

func NewClient(args ...string) (*TestClient, error) {
	addr := fmt.Sprintf("localhost:%d", Port)
	args = append([]string{"-grpc", addr},
		args...)
	cl := &TestClient{wait: make(chan error)}
	cl.cmd = exec.Command(Codesearch, args...)

	cl.cmd.Stderr = os.Stderr

	if e := cl.cmd.Start(); e != nil {
		return nil, e
	}

	go func() {
		cl.wait <- cl.cmd.Wait()
	}()

	conn, err := grpc.Dial(addr, grpc.WithInsecure(), grpc.WithBlock())
	if err != nil {
		cl.Close()
		return nil, err
	}

	cl.client = pb.NewCodeSearchClient(conn)

	return cl, nil
}
