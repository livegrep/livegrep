package backend

import (
	"github.com/golang/glog"
	"github.com/livegrep/livegrep/client"
	"github.com/livegrep/livegrep/server/config"
	"sync"
	"time"
)

const (
	PoolSize = 8
)

type Repo struct {
	Name   string
	Github string
}

type I struct {
	Name  string
	Repos []Repo
	sync.Mutex
}

type Backend struct {
	Addr    string
	Id      string
	I       *I
	Clients chan client.Client
	pending chan struct{}
}

func New(cfg *config.Backend) *Backend {
	bk := &Backend{
		Addr:    cfg.Addr,
		Id:      cfg.Id,
		I:       &I{Name: cfg.Name},
		Clients: make(chan client.Client, PoolSize),
		pending: make(chan struct{}, PoolSize),
	}
	for _, r := range cfg.Repos {
		bk.I.Repos = append(bk.I.Repos, Repo{Name: r.Name, Github: r.Github})
	}
	for i := 0; i < PoolSize; i++ {
		bk.pending <- struct{}{}
	}
	go bk.connectLoop()
	return bk
}

func (bk *Backend) CheckIn(c client.Client) {
	if c.Err() != nil {
		c.Close()
		bk.pending <- struct{}{}
		return
	}

	bk.Clients <- c
}

func (bk *Backend) connectLoop() {
	for _ = range bk.pending {
		for {
			cl, err := client.Dial("tcp", bk.Addr)
			if err != nil {
				glog.Infof("Connection error: %s", err.Error())
				time.Sleep(5 * time.Second)
				continue
			}
			glog.Infof("Connected, backend=%s addr=%s",
				bk.Id, bk.Addr)

			if info := cl.Info(); info != nil {
				bk.refresh(info)
			}
			bk.Clients <- cl
			break
		}
	}
}

func (bk *Backend) refresh(info *client.ServerInfo) {
	bk.I.Lock()
	defer bk.I.Unlock()

	if info.Name != "" {
		bk.I.Name = info.Name
	}
	if len(info.Repos) > 0 {
		bk.I.Repos = nil
		for _, r := range info.Repos {
			gh := ""
			v, ok := r.Metadata["github"]
			if ok {
				gh = v.(string)
			}
			bk.I.Repos = append(bk.I.Repos,
				Repo{r.Name, gh})
		}
	}
}
