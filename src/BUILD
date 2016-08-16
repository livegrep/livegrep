cc_library(
  name = "codesearch",
  srcs = glob([
    "*.cc",
  ]),
  hdrs = glob(["*.h"]),
  deps = [
    "//src/lib",

    "@com_googlesource_code_re2//:re2",
    "@divsufsort//:divsufsort",
    "@boost//:intrusive_ptr",
    "@boost//:filesystem",
    "@com_github_sparsehash//:sparsehash",
    "@com_github_json_c//:json",
    "@com_github_libgit2//:libgit2",

    "//third_party:utf8cpp",
   ],
  copts = [ "-Wno-sign-compare" ],
  visibility = ["//visibility:public"],
)
