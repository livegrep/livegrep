workspace(name = "com_github_livegrep_livegrep")

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)
load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "build_stack_rules_proto",
    sha256 = "36f11f56f6eb48a81eb6850f4fb6c3b4680e3fc2d3ceb9240430e28d32c47009",
    strip_prefix = "rules_proto-d86ca6bc56b1589677ec59abfa0bed784d6b7767",
    urls = ["https://github.com/stackb/rules_proto/archive/d86ca6bc56b1589677ec59abfa0bed784d6b7767.tar.gz"],
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "divsufsort",
    build_file = "//third_party:BUILD.divsufsort",
    patch_args = ["-p1"],
    patches = ["//third_party:divsufsort.patch"],
    sha256 = "9164cb6044dcb6e430555721e3318d5a8f38871c2da9fd9256665746a69351e0",
    strip_prefix = "libdivsufsort-2.0.1",
    type = "tgz",
    url = "https://codeload.github.com/y-256/libdivsufsort/tar.gz/2.0.1",
)

git_repository(
    name = "com_googlesource_code_re2",
    commit = "767de83bb7e4bfe3a2d8aec0ec79f9f1f66da30a",
    remote = "https://github.com/google/re2",
)

git_repository(
    name = "gflags",
    commit = "e171aa2d15ed9eb17054558e0b3a6a413bb01067",  # v2.2.2
    remote = "https://github.com/gflags/gflags",
)

git_repository(
    name = "com_github_nelhage_rules_boost",
    commit = "c1d618315fa152958baef8ea0d77043eebf7f573",
    remote = "https://github.com/nelhage/rules_boost",
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

git_repository(
    name = "com_google_absl",
    commit = "5e0dcf72c64fae912184d2e0de87195fe8f0a425",
    remote = "https://github.com/abseil/abseil-cpp",
)

git_repository(
    name = "io_bazel_rules_go",
    commit = "dad6b2e97e4e81d364608a80acf38fc058d155a4",  # 0.18.0
    remote = "https://github.com/bazelbuild/rules_go.git",
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "c0a5739d12c6d05b6c1ad56f2200cb0b57c5a70e03ebd2f7b87ce88cabf09c7b",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.14.0/bazel-gazelle-0.14.0.tar.gz"],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies()

load(
    "//tools/build_defs:go_externals.bzl",
    "go_externals",
)

go_externals()

load(
    "//tools/build_defs:libgit2.bzl",
    "new_libgit2_archive",
)

new_libgit2_archive(
    name = "com_github_libgit2",
    build_file = "//third_party:BUILD.libgit2",
    sha256 = "0269ec198c54e44f275f8f51e7391681a03aa45555e2ab6ce60b0757b6bde3de",
    url = "https://github.com/libgit2/libgit2/archive/v0.24.1.tar.gz",
    version = "0.24.1",
)

load(
    "@build_stack_rules_proto//cpp:deps.bzl",
    "cpp_grpc_compile",
    "cpp_proto_compile",
)

cpp_proto_compile()

cpp_grpc_compile()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

git_repository(
    name = "io_bazel_buildifier",
    commit = "ae772d29d07002dfd89ed1d9ff673a1721f1b8dd",
    remote = "https://github.com/bazelbuild/buildifier.git",
)

git_repository(
    name = "org_dropbox_rules_node",
    commit = "f35666fe95bff69b9c96b95a97973ee5bef108bd",
    remote = "https://github.com/dropbox/rules_node.git",
)

load("@org_dropbox_rules_node//node:defs.bzl", "node_repositories")

node_repositories()

new_git_repository(
    name = "compdb",
    build_file_content = (
        """
package(default_visibility = ["//visibility:public"])
"""
    ),
    commit = "7bc80f9355b09466fffabce24d463d65e37fcc0f",
    remote = "https://github.com/grailbio/bazel-compilation-database.git",
)

git_repository(
    name = "com_google_googletest",
    commit = "0ea2d8f8fa1601abb9ce713b7414e7b86f90bc61",
    remote = "https://github.com/google/googletest",
)
