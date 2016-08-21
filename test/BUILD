cc_test(
    name = "codesearch_test",
    srcs = [ "codesearch_test.cc" ],
    deps = [
      "//src:codesearch",
      "@com_google_gtest//:main",
    ],
    defines = [
        "GTEST_HAS_TR1_TUPLE",
        "GTEST_USE_OWN_TR1_TUPLE=0",
    ],
    size = "small",
)
