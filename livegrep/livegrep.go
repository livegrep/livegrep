package main

import (
	"encoding/json"
	_ "expvar"
	"flag"
	"io/ioutil"
	"net/http"
	_ "net/http/pprof"
	"os"

	"github.com/golang/glog"
	"github.com/livegrep/livegrep/server"
	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/middleware"
)

var (
	serveAddr = flag.String("listen", "127.0.0.1:8910", "The address to listen on")
	docRoot   = flag.String("docroot", "./web", "The livegrep document root (web/ directory)")
)

// var backendAddr *string = flag.String("connect", "localhost:9999", "The address to connect to")

func main() {
	flag.Parse()

	if len(flag.Args()) == 0 {
		glog.Fatalf("Usage: %s CONFIG.json", os.Args[0])
	}

	data, err := ioutil.ReadFile(flag.Arg(0))
	if err != nil {
		glog.Fatalf(err.Error())
	}

	cfg := &config.Config{
		DocRoot: *docRoot,
		Listen:  *serveAddr,
	}
	if err = json.Unmarshal(data, &cfg); err != nil {
		glog.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
	}

	handler, err := server.New(cfg)
	if err != nil {
		panic(err.Error())
	}

	if cfg.ReverseProxy {
		handler = middleware.UnwrapProxyHeaders(handler)
	}

	http.DefaultServeMux.Handle("/", handler)

	glog.Infof("Listening on %s.", cfg.Listen)
	glog.Fatal(http.ListenAndServe(cfg.Listen, nil))
}
