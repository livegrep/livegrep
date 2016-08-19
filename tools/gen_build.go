package main

import (
	"bytes"
	"flag"
	"fmt"
	"go/build"
	"html/template"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

var tpl *template.Template

var (
	packagePrefix = flag.String("prefix",
		"github.com/livegrep/livegrep",
		"workspace root package")
)

func init() {
	tpl = template.New("build")
	/*
		tpl.Funcs(map[string]interface{}{
			"wantPackage": func(pkg string) bool {
				return strings.HasPrefix(pkg, *packagePrefix)
			},
			"toBazel": func(pkg string) string {
				return fmt.Sprintf("/%s:go_default_library",
					strings.TrimPrefix(pkg, *packagePrefix))
			},
		})
	*/
	template.Must(tpl.Parse(BuildTemplate))
}

func main() {
	flag.Parse()
	if len(flag.Args()) < 1 {
		log.Fatalf("usage: %s ROOT", os.Args[0])
	}
	dirs, err := collectDirs(flag.Args()[0])
	if err != nil {
		log.Fatal("collect:", err)
	}
	for _, d := range dirs {
		fmt.Printf("%s...\n", d)
	}

	ctx := build.Default

	for _, d := range dirs {
		if e := genBuild(&ctx, d); e != nil {
			fmt.Print("!", e)
		}
	}
}

func extractDeps(ctx *build.Context, srcDir string, imports []string) ([]string, error) {
	var want []string
	for _, i := range imports {
		if strings.HasPrefix(i, *packagePrefix) {
			want = append(want, i)
			continue
		}
		imported, e := ctx.Import(i, srcDir, 0)
		if e != nil {
			return nil, fmt.Errorf("import %s: %v", i, e)
		}
		if strings.HasPrefix(imported.ImportPath, *packagePrefix) {
			want = append(want, imported.ImportPath)
			continue
		}
	}
	var deps []string
	for _, pkg := range want {
		deps = append(deps,
			fmt.Sprintf("/%s:go_default_library",
				strings.TrimPrefix(pkg, *packagePrefix)))
	}
	sort.Strings(deps)
	return deps, nil
}

func genBuild(ctx *build.Context, d string) error {
	pkg, e := ctx.ImportDir(d, 0)
	if e != nil {
		return e
	}
	root, e := filepath.Abs(pkg.Dir)
	if e != nil {
		return e
	}
	deps, e := extractDeps(ctx, root, pkg.Imports)
	if e != nil {
		return e
	}
	testDeps, e := extractDeps(ctx, root, pkg.TestImports)
	if e != nil {
		return e
	}
	context := Context{
		GoFiles:     pkg.GoFiles,
		TestGoFiles: pkg.TestGoFiles,

		Deps:     deps,
		TestDeps: testDeps,

		IsBinary: pkg.IsCommand(),
		Package:  pkg.Name,
		DirName:  filepath.Base(pkg.Dir),
	}

	var buf bytes.Buffer
	e = tpl.Execute(&buf, &context)
	if e != nil {
		return e
	}
	return ioutil.WriteFile(filepath.Join(d, "BUILD"), buf.Bytes(), 0644)
}

func collectDirs(root string) ([]string, error) {
	out := make(map[string]struct{})
	e := filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
		if !info.IsDir() && strings.HasSuffix(path, ".go") {
			out[filepath.Dir(path)] = struct{}{}
		}
		return nil
	})
	if e != nil {
		return nil, e
	}
	var dirs []string
	for d := range out {
		if d == "tools" {
			continue
		}
		dirs = append(dirs, d)
	}
	return dirs, nil
}

type Context struct {
	IsBinary bool
	Package  string
	DirName  string

	GoFiles     []string
	TestGoFiles []string

	Deps     []string
	TestDeps []string
}

const BuildTemplate = `
load("@io_bazel_rules_go//go:def.bzl",
  "go_binary",
  "go_library",
  "go_test",
)

{{if .IsBinary -}}
go_binary(
{{else -}}
go_library(
{{end -}}
  name = "{{if .IsBinary }}{{.DirName}}{{else}}go_default_library{{end}}",
  srcs = [
{{- range .GoFiles }}
    "{{.}}",
{{- end}}
  ],
  deps = [
{{- range .Deps }}
    "{{.}}",
{{- end }}
  ],
  visibility = ["//visibility:public"],
)

{{if .TestGoFiles}}
go_test(
  name = "go_default_test",
  srcs = [
  {{- range .TestGoFiles}}
    "{{.}}",
  {{- end}}
  ],
  deps = [
{{- range .TestDeps }}
    "{{.}}",
{{- end }}
  ],
  library = ":go_default_library",
)
{{end}}
`
