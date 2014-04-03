package main

import (
	"encoding/json"
	"flag"
	"github.com/golang/glog"
	"github.com/nelhage/livegrep/server"
	"github.com/nelhage/livegrep/server/config"
	"github.com/nelhage/livegrep/server/middleware"
	"io/ioutil"
	"net/http"
	"os"
)

var (
	serveAddr  *string = flag.String("listen", "127.0.0.1:8910", "The address to listen on")
	docRoot    *string = flag.String("docroot", "./web", "The livegrep document root (web/ directory)")
	production *bool   = flag.Bool("production", false, "Is livegrep running in production?")
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
		DocRoot:    *docRoot,
		Production: *production,
	}
	if err = json.Unmarshal(data, &cfg); err != nil {
		glog.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
	}

	handler, err := server.New(cfg)
	if err != nil {
		panic(err.Error())
	}

	if *production {
		handler = middleware.UnwrapProxyHeaders(handler)
	}

	glog.Fatal(http.ListenAndServe(*serveAddr, handler))
}
