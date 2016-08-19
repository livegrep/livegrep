cc_binary(
  name = "codesearch",
  srcs = [
    "codesearch.cc",
    "transport.cc",
    "transport.h",
  ],
  deps = [
    "//src:codesearch",
    "@boost//:bind",
    "@com_github_libgit2//:libgit2",
  ],
  copts = [
    "-Wno-deprecated-declarations",
    "-Wno-sign-compare",
  ]
)

cc_binary(
  name = "codesearchtool",
  srcs = [
    "codesearchtool.cc",
    "inspect-index.cc",
    "analyze-re.cc",
    "dump-file.cc",
  ],
  deps = [
    "//src:codesearch",
  ],
  copts = [
    "-Wno-sign-compare",
  ],
)

[genrule(
  name = "tool-" + t,
  srcs = [ ":codesearchtool" ],
  outs = [ t ],
  output_to_bindir = 1,
  cmd = "ln -nsf codesearchtool $@",
) for t in [ 'analyze-re', 'dump-file', 'inspect-index' ]]
