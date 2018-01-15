load("@io_bazel_rules_go//go:def.bzl", "go_prefix")
load("@compdb//:aspects.bzl", "compilation_database")

go_prefix("github.com/livegrep/livegrep")

compilation_database(
    name = "compilation_db",
    exec_root_marker = True,
    targets = [
        "//src/tools:codesearch",
        "//src/tools:codesearchtool",
    ],
)
