cc_library(
    name = "lib",
    srcs = glob(["*.cc"]),
    hdrs = glob(["*.h"]),
    copts = ["-Wno-sign-compare"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gflags_gflags//:gflags",
    ],
)
