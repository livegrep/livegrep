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
	return client.ClientWithRetry(func() (client.Client, error) {
		return client.Dial("tcp", bk.config.Addr)
	}), nil
}

func (b *backend) checkIn(c client.Client) {
	c.Close()
}
