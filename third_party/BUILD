cc_library(
    name = "utf8cpp",
    hdrs = [
        "utf8cpp/source/utf8.h",
        "utf8cpp/source/utf8/checked.h",
        "utf8cpp/source/utf8/core.h",
        "utf8cpp/source/utf8/unchecked.h",
    ],
    includes = ["utf8cpp/source"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "divsufsort",
    hdrs = glob(["divsufsort/*.h"]),
    strip_include_prefix = "divsufsort",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "libgit2_features",
    hdrs = glob(["libgit2/*.h"]),
    include_prefix = "git2/sys",
    strip_include_prefix = "libgit2",
    visibility = ["//visibility:public"],
)
