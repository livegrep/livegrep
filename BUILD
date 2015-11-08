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
    "//src/vendor/re2:re2",
    ":lib",
    "@divsufsort//:divsufsort",
    "//src/vendor:utf8cpp",
  ],
  hdrs = glob(["src/*.h"]),
  includes = [ "src" ],
)

cc_binary(
  name = "codesearch",
  srcs = [
    "src/tools/codesearch.cc",
    "src/tools/transport.cc",
  ],
  deps = [
    ":lib",
    ":libcodesearch",
  ],
  linkopts = [
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
  ],
)
