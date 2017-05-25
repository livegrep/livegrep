package templates

import (
	"html/template"
	"log"
	"net/http"
	"path"
	"reflect"
	"strings"
	texttemplate "text/template"
)

func templatePath(f reflect.StructField) string {
	if path := f.Tag.Get("template"); path != "" {
		return path
	}
	return strings.ToLower(f.Name) + ".html"
}

func getFuncs() map[string]interface{} {
	return map[string]interface{}{
		"loop":      func(n int) []struct{} { return make([]struct{}, n) },
		"toLineNum": func(n int) int { return n + 1 },
	}
}

func Load(base string, templates interface{}) error {
	v := reflect.ValueOf(templates)
	if v.Kind() != reflect.Ptr {
		panic("Load: Must provide pointer-to-struct")
	}
	v = v.Elem()
	if v.Kind() != reflect.Struct {
		panic("Load: Must provide pointer-to-struct")
	}
	t := v.Type()
	for i := 0; i < t.NumField(); i++ {
		f := t.Field(i)

		is_html_template := f.Type.AssignableTo(reflect.TypeOf((*template.Template)(nil)))
		is_text_template := f.Type.AssignableTo(reflect.TypeOf((*texttemplate.Template)(nil)))
		if !is_html_template && !is_text_template {
			continue
		}

		p := templatePath(f)
		name := path.Base(p)
		var err error
		var tpl interface{}
		if is_html_template {
			tpl, err = template.New(name).Funcs(getFuncs()).ParseFiles(path.Join(base, p))
		} else {
			tpl, err = texttemplate.New(name).Funcs(getFuncs()).ParseFiles(path.Join(base, p))
		}

		if err != nil {
			return err
		}
		v.Field(i).Set(reflect.ValueOf(tpl))
	}
	return nil
}

type reloadHandler struct {
	baseDir string
	t       interface{}
	in      http.Handler
}

func ReloadHandler(base string, templates interface{}, h http.Handler) http.Handler {
	return &reloadHandler{base, templates, h}
}

func (h *reloadHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	e := Load(h.baseDir, h.t)
	if e != nil {
		log.Printf("loading templates: err=%v", e)
	}
	h.in.ServeHTTP(w, r)
}
