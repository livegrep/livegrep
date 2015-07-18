// Implement some custom flag.Value instances for use in main.go
package main

import "strings"

type stringList struct {
	strings []string
}

func (s *stringList) String() string {
	return strings.Join(s.strings, ", ")
}

func (s *stringList) Set(str string) error {
	s.strings = append(s.strings, str)
	return nil
}

func (s *stringList) Get() interface{} {
	return s.strings
}

type dynamicDefault struct {
	val     string
	display string
	fn      func() string
}

func (d *dynamicDefault) String() string {
	if d.val != "" {
		return d.val
	}
	return d.display
}

func (d *dynamicDefault) Get() interface{} {
	if d.val != "" {
		return d.val
	}
	return d.fn()
}

func (d *dynamicDefault) Set(str string) error {
	d.val = str
	return nil
}
