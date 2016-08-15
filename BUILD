cc_library(
  name = "lib",
  srcs = glob(["src/lib/*.cc"]),
  hdrs = glob(["src/lib/*.h"]),
  deps = [ "@gflags//:gflags" ],
  copts = [ "-Wno-sign-compare" ],
)

cc_library(
  name = "libcodesearch",
  srcs = glob([
    "src/*.cc",
  ]),
  deps = [
    ":lib",

    "@com_googlesource_code_re2//:re2",
    "@divsufsort//:divsufsort",
    "@boost//:intrusive_ptr",
    "@boost//:filesystem",
    "@com_github_sparsehash//:sparsehash",
    "@com_github_json_c//:json",

    "//third_party:utf8cpp",
   ],
  hdrs = glob(["src/*.h"]),
  copts = [ "-Wno-sign-compare" ],
)

LIBS = [
    "-lm",
    "-lgit2",
    "-lz",
    "-lssl",
    "-lcrypto",
    "-ldl",
    "-lrt",
]

cc_binary(
  name = "codesearch",
  srcs = [
    "src/tools/codesearch.cc",
    "src/tools/transport.cc",
    "src/tools/transport.h",
  ],
  deps = [
    ":libcodesearch",
    "@boost//:bind",
  ],
  linkopts = LIBS,
  copts = [
    "-Wno-deprecated-declarations",
    "-Wno-sign-compare",
  ]
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
