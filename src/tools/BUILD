package(default_visibility = ["//visibility:public"])

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "GRPC_COMPILE_DEPS")
load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")

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
        "@boost//:bind",
    ] + GRPC_COMPILE_DEPS,
)

cc_binary(
    name = "codesearch",
    srcs = [
        "codesearch.cc",
        "limits.h",
        "transport.cc",
        "transport.h",
    ],
    copts = [
        "-Wno-deprecated-declarations",
        "-Wno-sign-compare",
    ],
    deps = [
        ":grpc_server",
        "//src:codesearch",
        "//src/proto:cc_proto",
        "@boost//:bind",
        "@com_github_libgit2//:libgit2",
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
