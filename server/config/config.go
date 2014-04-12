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
	// Location of the directory containing templates and static
	// assets. This should point at the "web" directory of the
	// repository.
	DocRoot string `json:"docroot"`

	Feedback struct {
		// The mailto address for the "feedback" url.
		Mailto string `json:"mailto"`
	} `json:"feedback"`

	GoogleAnalyticsId string `json:"google_analytics_id"`
	// Should we respect X-Real-Ip, X-Real-Proto, and X-Forwarded-Host?
	ReverseProxy bool `json:"reverse_proxy"`

	// List of backends to connect to. Each backend must minimally
	// include the "id" and "addr" fields; All other fields are
	// optional and will be replaced by values reported by the
	// backend server once we successfully connect.
	Backends []Backend `json:"backends"`
}
