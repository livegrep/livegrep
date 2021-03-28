package config

import (
	"html/template"
)

type Backend struct {
	Id   string `json:"id"`
	Addr string `json:"addr"`
}

type Honeycomb struct {
	WriteKey string `json:"write_key"`
	Dataset  string `json:"dataset"`
}

type Config struct {
	// Location of the directory containing templates and static
	// assets. This should point at the "web" directory of the
	// repository.
	DocRoot string `json:"docroot"`

	Feedback struct {
		// The mailto address for the "feedback" url.
		MailTo string `json:"mailto"`
	} `json:"feedback"`

	GoogleAnalyticsId string `json:"google_analytics_id"`
	// Should we respect X-Real-Ip, X-Real-Proto, and X-Forwarded-Host?
	ReverseProxy bool `json:"reverse_proxy"`

	// List of backends to connect to. Each backend must include
	// the "id" and "addr" fields.
	Backends []Backend `json:"backends"`

	// The address to listen on, as HOST:PORT.
	Listen string `json:"listen"`

	// HTML injected into layout template
	// for site-specific customizations
	HeaderHTML template.HTML `json:"header_html"`

	// HTML injected into layout template
	// just before </body> for site-specific customization
	FooterHTML template.HTML `json:"footer_html"`

	Sentry struct {
		URI string `json:"uri"`
	} `json:"sentry"`

	// Whether to re-load templates on every request
	Reload bool `json:"reload"`

	// honeycomb API write key
	Honeycomb Honeycomb `json:"honeycomb"`

	DefaultMaxMatches int32 `json:"default_max_matches"`

	// Same json config structure that the backend uses when building indexes;
	// used here for repository browsing.
	IndexConfig IndexConfig `json:"index_config"`

	DefaultSearchRepos []string `json:"default_search_repos"`

	LinkConfigs []LinkConfig `json:"file_links"`
}

type IndexConfig struct {
	Name         string       `json:"name"`
	Repositories []RepoConfig `json:"repositories"`
}

type RepoConfig struct {
	Path           string            `json:"path"`
	Name           string            `json:"name"`
	Revisions      []string          `json:"revisions"`
	Metadata       map[string]string `json:"metadata"`
	WalkSubmodules bool              `json:"walk_submodules"`
}

type LinkConfig struct {
	Label            string `json:"label"`
	UrlTemplate      string `json:"url_template"`
	WhitelistPattern string `json:"whitelist_pattern"`
}
