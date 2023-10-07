load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")
load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")

refresh_compile_commands(
    name = "compilation_db",
    targets = [
        "//src/tools:codesearch",
        "//src/tools:codesearchtool",
        "//test:codesearch_test",
    ],
)

load("@bazel_gazelle//:def.bzl", "gazelle")

# gazelle:prefix github.com/livegrep/livegrep
gazelle(name = "gazelle")

filegroup(
    name = "docs",
    srcs = glob([
        "doc/**/*",
    ]),
)

pkg_tar(
    name = "livegrep",
    srcs = [
        ":COPYING",
        ":README.md",
        ":docs",
    ],
    strip_prefix = ".",
    deps = [
        "//cmd:go_tools",
        "//src/tools:cc_tools",
        "//web:assets",
    ],
)
