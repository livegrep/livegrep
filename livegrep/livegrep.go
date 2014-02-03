package main

import (
	"flag"
	"github.com/nelhage/livegrep/config"
	"github.com/nelhage/livegrep/server"
	"log"
	"net/http"
)

var (
	serveAddr  *string = flag.String("listen", "127.0.0.1:8910", "The address to listen on")
	docRoot    *string = flag.String("docroot", "./web", "The livegrep document root (web/ directory)")
	production *bool   = flag.Bool("production", false, "Is livegrep running in production?")
)

// var backendAddr *string = flag.String("connect", "localhost:9999", "The address to connect to")

func main() {
	flag.Parse()

	cfg := &config.Config{
		DocRoot:    *docRoot,
		Production: *production,
		Backends: []config.Backend{
			{
				Id:        "linux",
				Name:      "Linux 3.12",
				Addr:      "localhost:9999",
				IndexPath: "/home/nelhage/code/linux/codesearch.idx",
				Repos: []config.Repo{
					{
						Path:   "/home/nelhage/code/linux/",
						Name:   "",
						Refs:   []string{"v3.12"},
						Github: "torvalds/linux",
					},
				},
			},
		},
	}

	handler, err := server.New(cfg)
	if err != nil {
		panic(err.Error())
	}
	log.Fatal(http.ListenAndServe(*serveAddr, handler))
}
