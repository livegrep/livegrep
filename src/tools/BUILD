package(default_visibility = [ "//visibility:public" ])

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
    "@com_github_grpc_grpc//:grpc",
  ],
)

cc_binary(
  name = "codesearch",
  srcs = [
    "codesearch.cc",
    "transport.cc",
    "transport.h",
    "limits.h",
  ],
  deps = [
    ":grpc_server",
    "//src:codesearch",
    "//src/proto:cc_proto",

    "@boost//:bind",
    "@com_github_libgit2//:libgit2",
  ],
  copts = [
    "-Wno-deprecated-declarations",
    "-Wno-sign-compare",
  ],
)

cc_binary(
  name = "codesearchtool",
  srcs = [
    "codesearchtool.cc",
    "inspect-index.cc",
    "analyze-re.cc",
    "dump-file.cc",
  ],
  deps = [
    "//src:codesearch",
  ],
  copts = [
    "-Wno-sign-compare",
  ],
)

[genrule(
  name = "tool-" + t,
  srcs = [ ":codesearchtool" ],
  outs = [ t ],
  output_to_bindir = 1,
  cmd = "ln -nsf codesearchtool $@",
) for t in [ 'analyze-re', 'dump-file', 'inspect-index' ]]
