package main

import (
	"flag"
	"github.com/nelhage/livegrep/server"
	"log"
	"net/http"
)

var serveAddr *string = flag.String("listen", "127.0.0.1:8910", "The address to listen on")
var backendAddr *string = flag.String("connect", "localhost:9999", "The address to connect to")

func main() {
	flag.Parse()
	handler, err := server.Handler("tcp", *backendAddr)
	if err != nil {
		panic(err.Error())
	}
	log.Fatal(http.ListenAndServe(*serveAddr, handler))
}
