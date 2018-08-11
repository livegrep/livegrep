package langserver

import (
	"context"
	"net"

	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/log"
	"github.com/sourcegraph/jsonrpc2"
	"path/filepath"
	"time"
)

// infers a language server for a given file. Picks only one for simplicity.
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
	rpcClient *jsonrpc2.Conn
}

type handler struct{}

func (h handler) Handle(context.Context, *jsonrpc2.Conn, *jsonrpc2.Request) {}

func NewClient(ctx context.Context, address string) (client Client, err error) {
	codec := jsonrpc2.VSCodeObjectCodec{}
	conn, err := net.Dial("tcp", address)
	if err != nil {
		return
	}
	rpcConn := jsonrpc2.NewConn(ctx, jsonrpc2.NewBufferedStream(conn, codec), handler{})
	client = &langServerClientImpl{
		rpcClient: rpcConn,
	}
	return
}

func (ls *langServerClientImpl) Initialize(
	ctx context.Context,
	params *InitializeParams,
) (InitializeResult, error) {

	var result InitializeResult
	err := ls.call(ctx, "initialize", params, &result)
	if err != nil {
		// it could presumably be because we're already initialized.
		nerr := ls.notify(ctx, "initialized", nil)
		if nerr != nil {
			return result, nerr
		}
	}
	return result, err
}

func (ls *langServerClientImpl) JumpToDef(
	ctx context.Context,
	params *TextDocumentPositionParams,
) (result []Location, err error) {
	err = ls.call(ctx, "textDocument/definition", params, &result)
	return
}

func (ls *langServerClientImpl) call(ctx context.Context, method string, params interface{}, result interface{}) error {
	ctx, cancel := context.WithTimeout(ctx, 10*time.Second)
	defer cancel()
	start := time.Now()
	err := ls.rpcClient.Call(ctx, method, params, &result)
	log.Printf(ctx, "%s %s\nParams: %+v, Result: %+v, err: %+v\n", method, time.Since(start), params, result, err)
	return err
}

func (ls *langServerClientImpl) notify(ctx context.Context, method string, params interface{}) error {
	ctx, cancel := context.WithTimeout(ctx, 10*time.Second)
	defer cancel()
	return ls.rpcClient.Notify(ctx, method, params)
}
