package client

type retryClient struct {
	newClient func() (Client, error)
	client    Client
}

func ClientWithRetry(newClient func() (Client, error)) Client {
	r := &retryClient{
		newClient: newClient,
	}
	r.ensureClient()
	return r
}

func (c *retryClient) ensureClient() error {
	if c.client != nil {
		return nil
	}
	var err error
	c.client, err = c.newClient()
	return err
}

func (c *retryClient) Query(q *Query) (Search, error) {
	if err := c.ensureClient(); err != nil {
		return nil, err
	}

	s, e := c.client.Query(q)
	if e != nil {
		c.Close()
	}
	return s, e
}

func (c *retryClient) Err() error {
	return nil
}

func (c *retryClient) Info() *ServerInfo {
	if err := c.ensureClient(); err != nil {
		return nil
	}
	return c.client.Info()
}

func (c *retryClient) Close() {
	if c.client != nil {
		c.client.Close()
		c.client = nil
	}
}
