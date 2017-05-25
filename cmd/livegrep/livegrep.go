package main

import (
	"encoding/json"
	_ "expvar"
	"flag"
	"io/ioutil"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"

	libhoney "github.com/honeycombio/libhoney-go"
	"github.com/livegrep/livegrep/server"
	"github.com/livegrep/livegrep/server/config"
	"github.com/livegrep/livegrep/server/middleware"
)

var (
	serveAddr   = flag.String("listen", "127.0.0.1:8910", "The address to listen on")
	docRoot     = flag.String("docroot", "./web", "The livegrep document root (web/ directory)")
	indexConfig = flag.String("index-config", "", "Codesearch index config file; provide to enable repo browsing")
	reload      = flag.Bool("reload", false, "Reload template files on every request")
	_           = flag.Bool("logtostderr", false, "[DEPRECATED] compatibility with glog")
)

// var backendAddr *string = flag.String("connect", "localhost:9999", "The address to connect to")

func main() {
	flag.Parse()

	cfg := &config.Config{
		DocRoot: *docRoot,
		Listen:  *serveAddr,
		Reload:  *reload,
		Backends: []config.Backend{
			{Id: "", Addr: "localhost:9999"},
		},
		Honeycomb: config.Honeycomb{
			WriteKey: os.Getenv("HONEYCOMB_WRITE_KEY"),
			Dataset:  os.Getenv("HONEYCOMB_DATASET"),
		},
	}

	if *indexConfig != "" {
		data, err := ioutil.ReadFile(*indexConfig)
		if err != nil {
			log.Fatalf(err.Error())
		}

		if err = json.Unmarshal(data, &cfg.IndexConfig); err != nil {
			log.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
		}
	}

	if len(flag.Args()) != 0 {
		data, err := ioutil.ReadFile(flag.Arg(0))
		if err != nil {
			log.Fatalf(err.Error())
		}

		if err = json.Unmarshal(data, &cfg); err != nil {
			log.Fatalf("reading %s: %s", flag.Arg(0), err.Error())
		}
	}

	libhoney.Init(libhoney.Config{})

	handler, err := server.New(cfg)
	if err != nil {
		panic(err.Error())
	}

	if cfg.ReverseProxy {
		handler = middleware.UnwrapProxyHeaders(handler)
	}

	http.DefaultServeMux.Handle("/", handler)

	log.Printf("Listening on %s.", cfg.Listen)
	log.Fatal(http.ListenAndServe(cfg.Listen, nil))
}
