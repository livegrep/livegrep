config_setting(
    name = "darwin",
    values = {"host_cpu": "darwin"},
)

cc_test(
    name = "codesearch_test",
    srcs = [
      "codesearch_test.cc",
      "indexer_test.cc",
      "main.cc",
    ],
    deps = [
      "//src:codesearch",
      "//src/tools:grpc_server",
      "@gtest//:gtest",
    ],
    defines = select({
      ":darwin": [
        "GTEST_HAS_TR1_TUPLE=0",
        "GTEST_USE_OWN_TR1_TUPLE=1",
      ],
      "//conditions:default": [
        "GTEST_HAS_TR1_TUPLE",
        "GTEST_USE_OWN_TR1_TUPLE=0",
      ]}),
    size = "small",
)

test_suite(
    name = "test",
)
