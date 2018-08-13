workspace(name = "com_github_livegrep_livegrep")

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)

git_repository(
    name = "org_pubref_rules_protobuf",
    commit = "ff3b7e7963daa7cb3b42f8936bc11eda4b960926",
    remote = "https://github.com/pubref/rules_protobuf",
)

load(
    "//tools/build_defs:externals.bzl",
    "new_patched_http_archive",
)

new_patched_http_archive(
    name = "divsufsort",
    build_file = "//third_party:BUILD.divsufsort",
    patch_file = "//third_party:divsufsort.patch",
    sha256 = "9164cb6044dcb6e430555721e3318d5a8f38871c2da9fd9256665746a69351e0",
    strip_prefix = "libdivsufsort-2.0.1",
    type = "tgz",
    url = "https://codeload.github.com/y-256/libdivsufsort/tar.gz/2.0.1",
)

git_repository(
    name = "com_googlesource_code_re2",
    commit = "7cf8b88e8f70f97fd4926b56aa87e7f53b2717e0",
    remote = "https://github.com/google/re2",
)

git_repository(
    name = "gflags",
    commit = "a69b2544d613b4bee404988710503720c487119a",
    remote = "https://github.com/gflags/gflags",
)

git_repository(
    name = "com_github_nelhage_boost",
    commit = "d6446dc9de6e43b039af07482a9361bdc6da5237",
    remote = "https://github.com/nelhage/rules_boost",
)
# local_repository(
#   name = "com_github_nelhage_boost",
#   path = "../rules_boost",
# )

load(
    "@com_github_nelhage_boost//:boost/boost.bzl",
    "boost_deps",
)

boost_deps()

new_http_archive(
    name = "com_github_sparsehash",
    build_file = "//third_party:BUILD.sparsehash",
    sha256 = "05e986a5c7327796dad742182b2d10805a8d4f511ad090da0490f146c1ff7a8c",
    strip_prefix = "sparsehash-sparsehash-2.0.3/",
    url = "https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz",
)

new_patched_http_archive(
    name = "com_github_json_c",
    add_prefix = "json-c",
    build_file = "//third_party:BUILD.json_c",
    patch_file = "//third_party:json_c.patch",
    sha256 = "5a617da9aade997938197ef0f8aabd7f97b670c216dc173977e1d56eef9e1291",
    strip_prefix = "json-c-0.12.1",
    url = "https://s3.amazonaws.com/json-c_releases/releases/json-c-0.12.1-nodoc.tar.gz",
)

git_repository(
    name = "io_bazel_rules_go",
    commit = "44b3bdf7d3645cbf0cfd786c5f105d0af4cf49ca",
    remote = "https://github.com/bazelbuild/rules_go.git",
)

load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")

go_rules_dependencies()

go_register_toolchains()

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

load("@org_pubref_rules_protobuf//go:rules.bzl", "go_proto_repositories")
load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")

cpp_proto_repositories(excludes = [
    "com_google_protobuf",
    "org_golang_google_grpc",
])

git_repository(
    name = "io_bazel_buildifier",
    commit = "0ca1d7991357ae7a7555589af88930d82cf07c0a",
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
    commit = "02c33ed2c0e86053073080fd215f44356ef5b543",
    remote = "https://github.com/grailbio/bazel-compilation-database.git",
)
