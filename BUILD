cc_library(
  name = "lib",
  srcs = glob(["src/lib/*.cc"]),
  hdrs = glob(["src/lib/*.h"]),
  includes = [ "src/lib/" ],
)

cc_library(
  name = "libcodesearch",
  srcs = glob([
    "src/*.cc",
  ]),
  deps = [
    "@re2//re2-2015-11-01:re2",
    ":lib",
    "@divsufsort//:divsufsort",
    "//src/vendor:utf8cpp",
  ],
  hdrs = glob(["src/*.h"]),
  includes = [ "src" ],
)

LIBS = [
    "-lm",
    "-lgit2",
    "-ljson",
    "-lgflags",
    "-lz",
    "-lssl",
    "-lcrypto",
    "-ldl",
    "-lboost_system",
    "-lboost_filesystem",
    "-lrt",
]

cc_binary(
  name = "codesearch",
  srcs = [
    "src/tools/codesearch.cc",
    "src/tools/transport.cc",
  ],
  deps = [
    ":libcodesearch",
  ],
  linkopts = LIBS,
)

cc_binary(
  name = "codesearchtool",
  srcs = [
    "src/tools/codesearchtool.cc",
    "src/tools/inspect-index.cc",
    "src/tools/analyze-re.cc",
    "src/tools/dump-file.cc",
  ],
  deps = [
    ":libcodesearch",
  ],
  linkopts = LIBS,
)

[genrule(
  name = "tool-" + t,
  srcs = [ ":codesearchtool" ],
  outs = [ t ],
  output_to_bindir = 1,
  cmd = "ln -nsf codesearchtool $@",
) for t in [ 'analyze-re', 'dump-file', 'inspect-index' ]]

cc_test(
    name = "codesearch_test",
    srcs = [ "test/codesearch_test.cc" ],
    deps = [
        "@gtest//:main",
        ":libcodesearch"
    ],
    defines = [
        "GTEST_HAS_TR1_TUPLE",
        "GTEST_USE_OWN_TR1_TUPLE=0",
    ],
    linkopts = LIBS,
    size = "small",
)
