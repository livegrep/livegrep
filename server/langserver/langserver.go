package langserver

import (
	"context"
	"net"

	"github.com/fatih/pool"
	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/sourcegraph/jsonrpc2"
	"path/filepath"
	"time"
)

// infers a language server for a given file. Picks only one.
func ForFile(repo *config.RepoConfig, filePath string) *config.LangServer {
	fileExt := filepath.Ext(filePath)
	for _, langServer := range repo.LangServers {
		for _, ext := range langServer.Extensions {
			if ext == fileExt {
				return &langServer
			}
		}
	}
	return nil
}

type Client interface {
	Initialize(ctx context.Context, params *InitializeParams) (InitializeResult, error)
	JumpToDef(ctx context.Context, params *TextDocumentPositionParams) ([]Location, error)
}

type langServerClientImpl struct {
	connPool pool.Pool
}

type handler struct{}

func (h handler) Handle(context.Context, *jsonrpc2.Conn, *jsonrpc2.Request) {}

func NewClient(address string) (client Client, err error) {
	p, err := pool.NewChannelPool(2, 5, func() (net.Conn, error) { return net.Dial("tcp", address) })

	client = &langServerClientImpl{
		connPool: p,
	}
	return client, nil
}

func (ls *langServerClientImpl) Initialize(ctx context.Context, params *InitializeParams) (result InitializeResult, err error) {
	err = ls.invoke(ctx, "initialize", params, &result)
	if err != nil {
		return
	}

	err = ls.notify(ctx, "initialized", nil)
	return
}

func (ls *langServerClientImpl) JumpToDef(
	ctx context.Context,
	params *TextDocumentPositionParams,
) (result []Location, err error) {
	err = ls.invoke(ctx, "textDocument/definition", params, &result)
	return
}

func (ls *langServerClientImpl) performRPC(ctx context.Context, f func(*jsonrpc2.Conn) error) (error) {
	codec := jsonrpc2.VSCodeObjectCodec{}
	conn, err := ls.connPool.Get()
	defer conn.Close()
	if err != nil {
		return err
	}

	if err := f(jsonrpc2.NewConn(ctx, jsonrpc2.NewBufferedStream(conn, codec), handler{})); err != nil {
		if _, ok := err.(net.Error); ok {
			if pc, ok := conn.(*pool.PoolConn); ok {
				pc.MarkUnusable()
			}
		}

		return err
	}

	return nil
}

func (ls *langServerClientImpl) invoke(ctx context.Context, method string, params interface{}, result interface{}) error {
	ctx, cancel := context.WithTimeout(ctx, 60*time.Second)
	defer cancel()
	start := time.Now()

	return ls.performRPC(ctx, func(rpcClient *jsonrpc2.Conn) error {
		err := rpcClient.Call(ctx, method, params, &result)
		log.Printf(ctx, "%s %s\nParams: %+v, Result: %+v, err: %+v\n", method, time.Since(start), params, result, err)
		return err
	})
}

func (ls *langServerClientImpl) notify(ctx context.Context, method string, params interface{}) error {
	ctx, cancel := context.WithTimeout(ctx, 60*time.Second)
	defer cancel()
	start := time.Now()

	return ls.performRPC(ctx, func(rpcClient *jsonrpc2.Conn) error {
		err := rpcClient.Notify(ctx, method, params)
		log.Printf(ctx, "notify %s %s\nParams: %+v err: %+v\n", method, time.Since(start), params, err)
		return err
	})

}
