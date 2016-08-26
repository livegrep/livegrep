cc_test(
    name = "codesearch_test",
    srcs = [
      "codesearch_test.cc",
      "main.cc",
    ],
    deps = [
      "//src:codesearch",
      "@gtest//:gtest",
    ],
    defines = [
        "GTEST_HAS_TR1_TUPLE",
        "GTEST_USE_OWN_TR1_TUPLE=0",
    ],
    size = "small",
)
