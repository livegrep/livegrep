workspace(name = "com_github_livegrep_livegrep")

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)

git_repository(
    name = "org_pubref_rules_protobuf",
    commit = "master",
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

http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "c8ab833081c9766ef4e4d1e6397044ff3b20e42be109084b50d49c161f876184",
    strip_prefix = "re2-2018-02-01",
    url = "https://github.com/google/re2/archive/2018-02-01.tar.gz",
)

http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "ae27cdbcd6a2f935baa78e4f21f675649271634c092b1be01469440495609d0e",
    strip_prefix = "gflags-2.2.1",
    url = "https://github.com/gflags/gflags/archive/v2.2.1.tar.gz",
)

git_repository(
    name = "com_github_nelhage_boost",
    commit = "for-livegrep",
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

new_patched_http_archive(
    name = "com_github_sparsehash",
    build_file = "//third_party:BUILD.sparsehash",
    patch_file = "//third_party:sparsehash.patch",
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
    commit = "0.7.1",
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
    commit = "4fe6494f3f8d1a272d47d32ecc66698f6c43ed09",
    remote = "https://github.com/dropbox/rules_node.git",
)

load("@org_dropbox_rules_node//node:defs.bzl", "node_repositories")

node_repositories()

new_http_archive(
    name = "compdb",
    build_file_content = """
        package(default_visibility = ["//visibility:public"])
    """,
    sha256 = "d4aa11e8db119ffffcf46da2468a74808c3e5aae45281e688fe1ae70e27943c7",
    strip_prefix = "bazel-compilation-database-0.2",
    url = "https://github.com/grailbio/bazel-compilation-database/archive/0.2.tar.gz",
)
