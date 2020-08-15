cc_library(
    name = "lib",
    srcs = [
        "debug.cc",
        "metrics.cc",
        "radix_sort.cc",
    ] + select({
        "@bazel_tools//src/conditions:linux_x86_64": ["fs_linux.cc"],
        "//conditions:default": ["fs_default.cc"],
    }),
    hdrs = glob(["*.h"]),
    copts = ["-Wno-sign-compare"],
    visibility = ["//visibility:public"],
    deps = ["@gflags"],
)
