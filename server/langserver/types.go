package langserver

type ClientCapabilities struct{}

type ServerCapabilities struct{}

type InitializeParams struct {
	ProcessId        *int               `json:"processId"`
	OriginalRootPath string             `json:"originalRootPath"`
	RootPath         string             `json:"rootPath"`
	RootUri          string             `json:"rootUri"`
	Capabilities     ClientCapabilities `json:"capabilities"`
}

type InitializeResult struct {
	Capabilities ServerCapabilities `json:"capabilities"`
}

type SymbolInformation struct {
	Name          string   `json:"name"`
	Kind          int      `json:"kind"`
	Location      Location `json:"location"`
	ContainerName string   `json:"containerName"`
}

type Location struct {
	URI       string `json:"uri"`
	TextRange Range  `json:"range"`
}

type Range struct {
	Start Position `json:"start"`
	End   Position `json:"end"`
}

type Position struct {
	Line      int `json:"line"`
	Character int `json:"character"`
}

type TextDocumentPositionParams struct {
	TextDocument TextDocumentIdentifier `json:"textDocument"`
	Position     Position               `json:"position"`
}

type TextDocumentIdentifier struct {
	URI string `json:"uri"`
}
