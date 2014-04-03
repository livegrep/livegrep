package config

type Repo struct {
	Path   string   `json:"path"`
	Name   string   `json:"name"`
	Refs   []string `json:"refs"`
	Github string   `json:"github"`
}

type Backend struct {
	Id        string `json:"id"`
	Name      string `json:"name"`
	Addr      string `json:"addr"`
	IndexPath string `json:"index"`
	Repos     []Repo `json:"repos"`
}

type Config struct {
	DocRoot           string
	Production        bool
	GoogleAnalyticsId string `json:"google_analytics_id"`
	Feedback          struct {
		MailTo string `json:"mailto"`
	} `json:"feedback"`
	Backends []Backend `json:"backends"`
}
