package server

import (
	"log"
	"sync"
	"time"

	"github.com/livegrep/livegrep/client"
)

const (
	defaultPoolSize = 8
)

var (
	// maximum time to wait after a failed connection
	// attempt. `var` not `const` so it can be retried by the
	// tests.
	maxBackoff = 10 * time.Second
)

type Tree struct {
	Name    string
	Version string
	Github  string
}

type I struct {
	Name  string
	Trees []Tree
	sync.Mutex
}

type Backend struct {
	Id       string
	Dial     func() (client.Client, error)
	PoolSize int
	I        *I
	Clients  chan client.Client

	pending chan struct{}
	backoff time.Duration
}

func (bk *Backend) Start() {
	if bk.PoolSize == 0 {
		bk.PoolSize = defaultPoolSize
	}
	bk.Clients = make(chan client.Client, bk.PoolSize)
	bk.pending = make(chan struct{}, bk.PoolSize)
	bk.backoff = 10 * time.Millisecond
	if bk.I == nil {
		bk.I = &I{Name: bk.Id}
	}
	for i := 0; i < bk.PoolSize; i++ {
		bk.pending <- struct{}{}
	}
	go bk.connectLoop()
}

func (bk *Backend) CheckIn(c client.Client) {
	if c.Err() != nil {
		c.Close()
		bk.pending <- struct{}{}
		return
	}

	bk.Clients <- c
}

func (bk *Backend) Close() {
drain:
	for {
		select {
		case <-bk.pending:
		default:
			break drain
		}
	}
	close(bk.pending)
	for c := range bk.Clients {
		c.Close()
	}
}

func (bk *Backend) connectLoop() {
	for _ = range bk.pending {
	retry:
		cl, err := bk.Dial()
		if err != nil {
			log.Printf("Connection error: backend=%s error=%s",
				bk.Id, err.Error())
			bk.backoff *= 2
			if bk.backoff > maxBackoff {
				bk.backoff = maxBackoff
			}
			time.Sleep(bk.backoff)
			goto retry
		}

		log.Printf("Connected: backend=%s", bk.Id)
		bk.backoff = 10 * time.Millisecond

		if info := cl.Info(); info != nil {
			bk.refresh(info)
		}
		bk.Clients <- cl

	}
	close(bk.Clients)
}

func (bk *Backend) refresh(info *client.ServerInfo) {
	bk.I.Lock()
	defer bk.I.Unlock()

	if info.Name != "" {
		bk.I.Name = info.Name
	}
	if len(info.Trees) > 0 {
		bk.I.Trees = nil
		for _, r := range info.Trees {
			gh := ""
			if v, ok := r.Metadata["github"]; ok {
				gh = v.(string)
			}
			bk.I.Trees = append(bk.I.Trees,
				Tree{r.Name, r.Version, gh})
		}
	}
}
