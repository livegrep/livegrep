package config

type Repo struct {
	Path   string
	Name   string
	Refs   []string
	Github string
}

type Backend struct {
	Id        string
	Name      string
	Addr      string
	IndexPath string
	Repos     []Repo
}

type Config struct {
	DocRoot    string
	Production bool
	Backends   []Backend
}
