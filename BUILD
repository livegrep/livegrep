load("@compdb//:aspects.bzl", "compilation_database")

compilation_database(
    name = "compilation_db",
    exec_root_marker = True,
    targets = [
        "//src/tools:codesearch",
        "//src/tools:codesearchtool",
    ],
)

load("@bazel_gazelle//:def.bzl", "gazelle")

# gazelle:prefix github.com/livegrep/livegrep
gazelle(name = "gazelle")
