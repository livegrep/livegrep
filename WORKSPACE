workspace(name = "com_github_livegrep_livegrep")

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
)
load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)

http_archive(
    name = "divsufsort",
    build_file = "//third_party:BUILD.divsufsort",
    sha256 = "9164cb6044dcb6e430555721e3318d5a8f38871c2da9fd9256665746a69351e0",
    strip_prefix = "libdivsufsort-2.0.1",
    type = "tgz",
    url = "https://codeload.github.com/y-256/libdivsufsort/tar.gz/2.0.1",
)

git_repository(
    name = "gflags",
    commit = "e171aa2d15ed9eb17054558e0b3a6a413bb01067",  # v2.2.2
    remote = "https://github.com/gflags/gflags",
    shallow_since = "1541971260 +0000",
)

git_repository(
    name = "com_github_nelhage_rules_boost",
    commit = "c1d618315fa152958baef8ea0d77043eebf7f573",
    remote = "https://github.com/nelhage/rules_boost",
    shallow_since = "1546641660 -0600",
)
# local_repository(
#   name = "com_github_nelhage_boost",
#   path = "../rules_boost",
# )

load(
    "@com_github_nelhage_rules_boost//:boost/boost.bzl",
    "boost_deps",
)

boost_deps()

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "d6b2513456fe2229811da7eb67a444be7785f5323c6708b38d851d2b51e54d83",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.30.0/rules_go-v0.30.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.30.0/rules_go-v0.30.0.zip",
    ],
)

git_repository(
    name = "bazel_gazelle",
    commit = "3ea1d64d6fe943dac06c341f9a265472bb99acd7",  # 0.24.0
    remote = "https://github.com/bazelbuild/bazel-gazelle.git",
    shallow_since = "1633971621 -0400",
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.17.6")

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies()

load(
    "//tools/build_defs:go_externals.bzl",
    "go_externals",
)

go_externals()

http_archive(
    name = "com_github_libgit2",
    build_file = "//third_party:BUILD.libgit2",
    sha256 = "adf17310b590e6e7618f070c742b5ee028aeeed2c60099bc4190c386b5060de1",
    strip_prefix = "libgit2-0.27.9/",
    url = "https://github.com/libgit2/libgit2/archive/v0.27.9.tar.gz",
)

git_repository(
    name = "com_github_grpc_grpc",
    commit = "591d56e1300b6d11948e1b821efac785a295989c",  # 1.44.0
    remote = "https://github.com/grpc/grpc.git",
    shallow_since = "1644573434 +0100"
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

git_repository(
    name = "io_bazel_buildifier",
    commit = "ae772d29d07002dfd89ed1d9ff673a1721f1b8dd",
    remote = "https://github.com/bazelbuild/buildifier.git",
)

local_repository(
    name = "org_dropbox_rules_node",
    path = "tools/org_dropbox_rules_node",
)

load("@org_dropbox_rules_node//node:defs.bzl", "node_repositories")

node_repositories()

git_repository(
    name = "com_grail_bazel_compdb",
    commit = "7658de071fcd072163c24cc96d78e9891d4d81f5",
    remote = "https://github.com/grailbio/bazel-compilation-database.git",
)

git_repository(
    name = "com_google_googletest",
    commit = "0ea2d8f8fa1601abb9ce713b7414e7b86f90bc61",
    remote = "https://github.com/google/googletest",
)
