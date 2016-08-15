LIBS = [
    "-lm",
    "-lgit2",
    "-lz",
    "-lssl",
    "-lcrypto",
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
