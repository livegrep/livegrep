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
	DocRoot  string
	Feedback struct {
		MailTo string `json:"mailto"`
	} `json:"feedback"`

	GoogleAnalyticsId string `json:"google_analytics_id"`
	// Should we respect X-Real-Ip, X-Real-Proto, and X-Forwarded-Host?
	ReverseProxy bool `json:"reverse_proxy"`

	Backends []Backend `json:"backends"`
}
