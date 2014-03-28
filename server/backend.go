package server

import (
	"github.com/nelhage/livegrep/client"
	"github.com/nelhage/livegrep/config"
)

type backend struct {
	config  *config.Backend
	clients chan client.Client
}

func (bk *backend) connect() (client.Client, error) {
	select {
	case cl := <-bk.clients:
		return cl, nil
	default:
		return client.ClientWithRetry(func() (client.Client, error) {
			return client.Dial("tcp", bk.config.Addr)
		}), nil
	}
}

func (bk *backend) checkIn(c client.Client) {
	select {
	case bk.clients <- c:
	default:
		c.Close()
	}
}
