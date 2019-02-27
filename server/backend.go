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
	IndexTime time.Time
}

type Backend struct {
	Id         string
	Addr       string
	I          *I
	Codesearch pb.CodeSearchClient
}

func NewBackend(id string, addr string) (*Backend, error) {
	client, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		return nil, err
	}
	bk := &Backend{
		Id:         id,
		Addr:       addr,
		I:          &I{Name: id},
		Codesearch: pb.NewCodeSearchClient(client),
	}
	return bk, nil
}

func (bk *Backend) Start() {
	if bk.I == nil {
		bk.I = &I{Name: bk.Id}
	}
	go bk.poll()
}

func (bk *Backend) poll() {
	for {
		info, e := bk.Codesearch.Info(context.Background(), &pb.InfoRequest{}, grpc.FailFast(false))
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
	bk.I.IndexTime = time.Unix(info.IndexTime, 0)
	if len(info.Trees) > 0 {
		bk.I.Trees = nil
		for _, r := range info.Trees {
			pattern := r.Metadata.UrlPattern
			if v := r.Metadata.Github; v != "" {
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
