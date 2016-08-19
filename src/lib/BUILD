cc_library(
  name = "lib",
  srcs = glob(["*.cc"]),
  hdrs = glob(["*.h"]),
  deps = [ "@gflags//:gflags" ],
  copts = [ "-Wno-sign-compare" ],
  visibility = ["//visibility:public"],
)
