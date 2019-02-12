config_setting(
    name = "darwin",
    values = {"host_cpu": "darwin"},
)

cc_test(
    name = "codesearch_test",
    size = "small",
    srcs = [
        "codesearch_test.cc",
        "main.cc",
        "planner_test.cc",
        "tagsearch_test.cc",
    ],
    defines = select({
        ":darwin": [
            "GTEST_HAS_TR1_TUPLE=0",
            "GTEST_USE_OWN_TR1_TUPLE=1",
        ],
        "//conditions:default": [
            "GTEST_HAS_TR1_TUPLE",
            "GTEST_USE_OWN_TR1_TUPLE=0",
        ],
    }),
    deps = [
        "//src:codesearch",
        "//src/tools:grpc_server",
        "@com_google_googletest//:gtest",
    ],
)

test_suite(
    name = "test",
)
