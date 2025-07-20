load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "grpc_server",
    srcs = [
        "grpc_server.cc",
        "grpc_server.h",
        "limits.h",
    ],
    deps = [
        "//src:codesearch",
        "//src/proto:cc_proto",
        "@boost.bind//:boost.bind",
        "@grpc//:grpc++",
        "@grpc//:grpc++_reflection",
    ],
)

cc_binary(
    name = "codesearch",
    srcs = [
        "codesearch.cc",
        "limits.h",
    ],
    copts = [
        "-Wno-deprecated-declarations",
        "-Wno-sign-compare",
    ],
    deps = [
        ":grpc_server",
        "//src:codesearch",
        "//src/proto:cc_config_proto",
        "//src/proto:cc_proto",
        "@boost.bind//:boost.bind",
        "@libgit2//:libgit2",
    ],
)

cc_binary(
    name = "codesearchtool",
    srcs = [
        "analyze-re.cc",
        "codesearchtool.cc",
        "dump-file.cc",
        "inspect-index.cc",
    ],
    copts = [
        "-Wno-sign-compare",
    ],
    deps = [
        "//src:codesearch",
    ],
)

[genrule(
    name = "tool-" + t,
    srcs = [":codesearchtool"],
    outs = [t],
    cmd = "ln -nsf codesearchtool $@",
    output_to_bindir = 1,
) for t in [
    "analyze-re",
    "dump-file",
    "inspect-index",
]]

pkg_tar(
    name = "cc_tools",
    srcs = [
        ":analyze-re",
        ":codesearch",
        ":codesearchtool",
        ":dump-file",
        ":grpc_server",
        ":inspect-index",
    ],
    package_dir = "bin/",
)
