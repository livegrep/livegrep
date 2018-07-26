package langserver

import (
	"context"
	"net"

	"github.com/jolestar/go-commons-pool"
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
	clientPool *pool.ObjectPool
}

type handler struct{}

func (h handler) Handle(context.Context, *jsonrpc2.Conn, *jsonrpc2.Request) {}

func NewClient(address string, initParams *InitializeParams) (client Client, err error) {
	config := pool.NewDefaultPoolConfig()
	config.MaxTotal = 3

	p := pool.NewObjectPool(context.Background(), pool.NewPooledObjectFactorySimple(
		func(ctx context.Context) (interface{}, error) {
			conn, err := net.Dial("tcp", address)
			if err != nil {
				return nil, err
			}
			codec := jsonrpc2.VSCodeObjectCodec{}
			rpcClient := jsonrpc2.NewConn(ctx, jsonrpc2.NewBufferedStream(conn, codec), handler{})

			if err := rpcClient.Call(ctx, "initialize", initParams, nil); err != nil {
				return nil, err
			}

			if err := rpcClient.Notify(ctx, "initialized", nil); err != nil {
				return nil, err
			}

			return rpcClient, nil
		}), config)

	if err != nil {
		return nil, err
	}

	client = &langServerClientImpl{
		clientPool: p,
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
	rpcClient, err := ls.clientPool.BorrowObject(ctx)
	if err != nil {
		return err
	}

	invalidated := false
	defer func() {
		if !invalidated {
			ls.clientPool.ReturnObject(ctx, rpcClient)
		}
	}()

	if err := f(rpcClient.(*jsonrpc2.Conn)); err != nil {
		if netError, ok := err.(net.Error); (ok && !netError.Temporary()) || err == jsonrpc2.ErrClosed {
			log.Printf(ctx, "connection unhealthy, closing")

			ls.clientPool.InvalidateObject(ctx, rpcClient)
			invalidated = true
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
