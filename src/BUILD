cc_library(
    name = "codesearch",
    srcs = glob([
        "*.cc",
    ]),
    hdrs = glob(["*.h"]),
    copts = [
        "-Wno-sign-compare",
        "-std=c++14",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/lib",
        "//src/proto:cc_config_proto",
        "//third_party:utf8cpp",
        "@boost//:filesystem",
        "@boost//:intrusive_ptr",
        "@com_github_libgit2//:libgit2",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
        "@divsufsort",
    ],
)
