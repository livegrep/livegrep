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
    commit = "03a871125484f8bea934761d5e8673f5d4979b57",
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
    name = "io_bazel_rules_go",
    sha256 = "099a9fb96a376ccbbb7d291ed4ecbdfd42f6bc822ab77ae6f1b5cb9e914e94fa",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.35.0/rules_go-v0.35.0.zip",
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

go_register_toolchains(version = "1.19.2")

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

go_repository(
    name = "org_golang_x_sys",
    importpath = "golang.org/x/sys",
    sum = "h1:q3i8TbbEz+JRD9ywIRlyRAQbM0qF7hu24q3teo2hbuw=",
    version = "v0.33.0",
)

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

http_archive(
    name = "com_google_absl",
    sha256 = "497ebdc3a4885d9209b9bd416e8c3f71e7a1fb8af249f6c2a80b7cbeefcd7e21",
    strip_prefix = "abseil-cpp-20230802.1/",
    url = "https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.1.zip",
)

git_repository(
    name = "com_github_grpc_grpc",
    commit = "591d56e1300b6d11948e1b821efac785a295989c",  # 1.44.0
    remote = "https://github.com/grpc/grpc.git",
    shallow_since = "1644573434 +0100",
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

http_archive(
    name = "hedron_compile_commands",
    sha256 = "ed5aea1dc87856aa2029cb6940a51511557c5cac3dbbcb05a4abd989862c36b4",
    strip_prefix = "bazel-compile-commands-extractor-e16062717d9b098c3c2ac95717d2b3e661c50608",
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/e16062717d9b098c3c2ac95717d2b3e661c50608.tar.gz",
)

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")

hedron_compile_commands_setup()

git_repository(
    name = "com_google_googletest",
    commit = "0ea2d8f8fa1601abb9ce713b7414e7b86f90bc61",
    remote = "https://github.com/google/googletest",
)

http_archive(
    name = "aspect_rules_js",
    sha256 = "66ecc9f56300dd63fb86f11cfa1e8affcaa42d5300e2746dba08541916e913fd",
    strip_prefix = "rules_js-1.13.0",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.13.0.tar.gz",
)

load("@aspect_rules_js//js:repositories.bzl", "rules_js_dependencies")

rules_js_dependencies()

load("@rules_nodejs//nodejs:repositories.bzl", "DEFAULT_NODE_VERSION", "nodejs_register_toolchains")

nodejs_register_toolchains(
    name = "nodejs",
    node_version = DEFAULT_NODE_VERSION,
)

load("@aspect_rules_js//npm:npm_import.bzl", "npm_translate_lock")

npm_translate_lock(
    name = "npm",
    npmrc = "//web:.npmrc",
    pnpm_lock = "//web:pnpm-lock.yaml",
    verify_node_modules_ignored = "//:.bazelignore",
)

load("@npm//:repositories.bzl", "npm_repositories")

npm_repositories()

http_archive(
    name = "rules_pkg",
    sha256 = "cad05f864a32799f6f9022891de91ac78f30e0fa07dc68abac92a628121b5b11",
    urls = [
        "https://github.com/bazelbuild/rules_pkg/releases/download/1.0.0/rules_pkg-1.0.0.tar.gz",
    ],
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()
