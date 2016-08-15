LIBS = [
    "-lm",
    "-lgit2",
    "-lz",
    "-ldl",
    "-lrt",
]

cc_test(
    name = "codesearch_test",
    srcs = [ "test/codesearch_test.cc" ],
    deps = [
        "@gtest//:main",
        "//src:codesearch"
    ],
    defines = [
        "GTEST_HAS_TR1_TUPLE",
        "GTEST_USE_OWN_TR1_TUPLE=0",
    ],
    linkopts = LIBS,
    size = "small",
)

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")

go_prefix("github.com/livegrep/livegrep")
