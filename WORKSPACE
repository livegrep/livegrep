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

git_repository(
    name = "org_pubref_rules_protobuf",
    commit = "5f6195e83e06db2fd110626b0f2dc64e345e6618",  # v0.8.2
    remote = "https://github.com/pubref/rules_protobuf",
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

http_archive(
    name = "com_github_sparsehash",
    build_file = "//third_party:BUILD.sparsehash",
    sha256 = "05e986a5c7327796dad742182b2d10805a8d4f511ad090da0490f146c1ff7a8c",
    strip_prefix = "sparsehash-sparsehash-2.0.3/",
    url = "https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz",
)

http_archive(
    name = "com_github_json_c",
    build_file = "//third_party:BUILD.json_c",
    patch_args = ["-p1"],
    patches = ["//third_party:json_c.patch"],
    sha256 = "5a617da9aade997938197ef0f8aabd7f97b670c216dc173977e1d56eef9e1291",
    strip_prefix = "json-c-0.12.1",
    url = "https://s3.amazonaws.com/json-c_releases/releases/json-c-0.12.1-nodoc.tar.gz",
)

git_repository(
    name = "io_bazel_rules_go",
    commit = "2d792dea8d22c552f455623bb15eb4f61fcb2f1b",  # 0.16.5
    remote = "https://github.com/bazelbuild/rules_go.git",
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "c0a5739d12c6d05b6c1ad56f2200cb0b57c5a70e03ebd2f7b87ce88cabf09c7b",
    urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.14.0/bazel-gazelle-0.14.0.tar.gz"],
)

load("@io_bazel_rules_go//go:def.bzl", "go_register_toolchains", "go_rules_dependencies")

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

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")

cpp_proto_repositories(excludes = [
    "com_google_protobuf",
    "org_golang_google_grpc",
])

git_repository(
    name = "io_bazel_buildifier",
    commit = "ae772d29d07002dfd89ed1d9ff673a1721f1b8dd",
    remote = "https://github.com/bazelbuild/buildifier.git",
)

git_repository(
    name = "org_dropbox_rules_node",
    commit = "74d8aeb40d079acdceb2380af2a72e29613a8fd6",
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
