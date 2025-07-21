cc_library(
    name = "codesearch",
    srcs = glob([
        "*.cc",
    ]),
    hdrs = glob(["*.h"]),
    copts = [
        "-Wno-sign-compare",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/lib",
        "//src/proto:cc_config_proto",
        "//third_party:utf8cpp",
        "@com_google_protobuf//:json_util",
        "@boost.filesystem//:boost.filesystem",
        "@boost.intrusive//:boost.intrusive",
        "@libgit2//:libgit2",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/hash",
        "@abseil-cpp//absl/strings",
        "@re2//:re2",
        "@divsufsort",
    ],
)
