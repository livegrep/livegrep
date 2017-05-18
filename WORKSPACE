workspace(name = "com_github_livegrep_livegrep")

load("@bazel_tools//tools/build_defs/repo:git.bzl",
     "git_repository", "new_git_repository")

git_repository(
  name = "org_pubref_rules_protobuf",
  remote = "https://github.com/pubref/rules_protobuf",
  commit = "1a559c005859642894aa7b5ebf94b61fe781fb1d",
)

load("//tools/build_defs:externals.bzl",
     "new_patched_http_archive",
)

new_patched_http_archive(
  name = "divsufsort",
  url = "https://codeload.github.com/y-256/libdivsufsort/tar.gz/2.0.1",
  sha256 = "9164cb6044dcb6e430555721e3318d5a8f38871c2da9fd9256665746a69351e0",
  build_file = "//third_party:BUILD.divsufsort",
  patch_file = "//third_party:divsufsort.patch",
  strip_prefix = "libdivsufsort-2.0.1",
  type = "tgz",
)

git_repository(
  name = "com_googlesource_code_re2",
  remote = "https://github.com/google/re2",
  commit = "b94b7cd42e9f02673cd748c1ac1d16db4052514c",
)

git_repository(
  name = "gflags",
  remote = "https://github.com/gflags/gflags",
  commit = "a69b2544d613b4bee404988710503720c487119a"
)

git_repository(
  name = "com_github_nelhage_boost",
  remote = "https://github.com/nelhage/rules_boost",
  commit = "ead0110ff90d5d90d2eb67e7e78f34f42d8486a1",
)
# local_repository(
#   name = "com_github_nelhage_boost",
#   path = "../rules_boost",
# )

load("@com_github_nelhage_boost//:boost/boost.bzl",
     "boost_deps")
boost_deps()

new_patched_http_archive(
  name = "com_github_sparsehash",
  url = "https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz",
  sha256 = "05e986a5c7327796dad742182b2d10805a8d4f511ad090da0490f146c1ff7a8c",
  build_file = "//third_party:BUILD.sparsehash",
  strip_prefix = "sparsehash-sparsehash-2.0.3/",
  patch_file = "//third_party:sparsehash.patch",
)

new_patched_http_archive(
  name = "com_github_json_c",
  url = "https://s3.amazonaws.com/json-c_releases/releases/json-c-0.12.1-nodoc.tar.gz",
  sha256 = "5a617da9aade997938197ef0f8aabd7f97b670c216dc173977e1d56eef9e1291",
  strip_prefix = "json-c-0.12.1",
  build_file = "//third_party:BUILD.json_c",
  patch_file = "//third_party:json_c.patch",
  add_prefix = "json-c",
)

git_repository(
    name = "io_bazel_rules_go",
    remote = "https://github.com/bazelbuild/rules_go.git",
    commit = "805fd1566500997379806373feb05e138a4dfe28",
)

load("@io_bazel_rules_go//go:def.bzl",
     "go_repositories", "new_go_repository")

go_repositories()

load("//tools/build_defs:go_externals.bzl",
     "go_externals")

go_externals()

load("//tools/build_defs:libgit2.bzl",
     "new_libgit2_archive",
)

new_libgit2_archive(
  name = "com_github_libgit2",
  url = "https://github.com/libgit2/libgit2/archive/v0.24.1.tar.gz",
  version = "0.24.1",
  sha256 = "60198cbb34066b9b5c1613d15c0479f6cd25f4aef42f7ec515cd1cc13a77fede",
  build_file = "//third_party:BUILD.libgit2",
)

load("@org_pubref_rules_protobuf//go:rules.bzl", "go_proto_repositories")
load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")

go_proto_repositories();
cpp_proto_repositories();

git_repository(
    name = "io_bazel_buildifier",
    commit = "0ca1d7991357ae7a7555589af88930d82cf07c0a",
    remote = "https://github.com/bazelbuild/buildifier.git",
)
