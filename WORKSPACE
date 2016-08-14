workspace(name = "com_github_livegrep_livegrep")

new_http_archive(
  name = "divsufsort",
  url = "https://codeload.github.com/y-256/libdivsufsort/tar.gz/2.0.1",
  sha256 = "9164cb6044dcb6e430555721e3318d5a8f38871c2da9fd9256665746a69351e0",
  build_file = "src/vendor/BUILD.divsufsort",
  strip_prefix = "libdivsufsort-2.0.1",
  type = "tgz",
)

git_repository(
  name = "com_googlesource_code_re2",
  remote = "git://github.com/google/re2",
  commit = "ec8dfdfa39233663779f01935124ecc36e840a03",
)

new_http_archive(
  name = "gtest",
  url = "https://googletest.googlecode.com/files/gtest-1.7.0.zip",
  sha256 = "247ca18dd83f53deb1328be17e4b1be31514cedfc1e3424f672bf11fd7e0d60d",
  build_file = "src/vendor/BUILD.gtest",
  strip_prefix = "gtest-1.7.0",
)

git_repository(
  name = "gflags",
  remote = "git://github.com/gflags/gflags",
  commit = "a69b2544d613b4bee404988710503720c487119a"
)

git_repository(
  name = "com_github_nelhage_boost",
  remote = "git://github.com/nelhage/rules_boost",
  commit = "724734c74898500804d39bce9b9f26595c0cbfaa",
)
# local_repository(
#   name = "com_github_nelhage_boost",
#   path = "../rules_boost",
# )

load("@com_github_nelhage_boost//:boost/boost.bzl",
     "boost_deps")
boost_deps()
