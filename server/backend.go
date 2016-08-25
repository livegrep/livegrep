package server

import (
	"context"
	"log"
	"net/url"
	"sync"
	"time"

	pb "github.com/livegrep/livegrep/src/proto/go_proto"
	"google.golang.org/grpc"
)

type Tree struct {
	Name    string
	Version string
	Url     string
}

type I struct {
	Name  string
	Trees []Tree
	sync.Mutex
}

type Backend struct {
	Id         string
	Addr       string
	I          *I
	Codesearch pb.CodeSearchClient
	Ready      chan struct{}

	client *grpc.ClientConn
}

func (bk *Backend) Start() {
	if bk.I == nil {
		bk.I = &I{Name: bk.Id}
	}
	bk.Ready = make(chan struct{})
	go bk.poll()
}

func (bk *Backend) poll() {
	for {
		if bk.Codesearch == nil {
			conn, err := grpc.Dial(bk.Addr, grpc.WithInsecure())
			if err != nil {
				log.Printf("Dial: %s: %v", bk.Addr, err)
				time.Sleep(30 * time.Second)
				continue
			}
			bk.Codesearch = pb.NewCodeSearchClient(conn)
			close(bk.Ready)

		}
		info, e := bk.Codesearch.Info(context.Background(), &pb.InfoRequest{})
		if e == nil {
			bk.refresh(info)
		} else {
			log.Printf("refresh %s: %v", bk.Id, e)
		}
		time.Sleep(60 * time.Second)
	}
}

func (bk *Backend) refresh(info *pb.ServerInfo) {
	bk.I.Lock()
	defer bk.I.Unlock()

	if info.Name != "" {
		bk.I.Name = info.Name
	}
	if len(info.Trees) > 0 {
		bk.I.Trees = nil
		for _, r := range info.Trees {
			pattern := ""
			if v, ok := r.Metadata["url-pattern"]; ok {
				pattern = v
			}
			if v, ok := r.Metadata["github"]; ok {
				value := v
				base := ""
				_, err := url.ParseRequestURI(value)
				if err != nil {
					base = "https://github.com/" + value
				} else {
					base = value
				}
				pattern = base + "/blob/{version}/{path}#L{lno}"
			}
			bk.I.Trees = append(bk.I.Trees,
				Tree{r.Name, r.Version, pattern})
		}
	}
}
