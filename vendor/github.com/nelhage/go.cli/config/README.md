# go.config -- parse configuration files using go's "flag" package

command-line options already provide a flexible and powerful way to
configure many applications, and go's "flag" package provides an easy
way to define command-line options and distribute their values into
arbitrary modules in your program.

go.config builds on that infrastructure to provide a lightweight
config-file mechanism to read configuration options from a file
directly into a flag.FlagSet.

Run

```
godoc github.com/nelhage/go.config
```

for more information.
