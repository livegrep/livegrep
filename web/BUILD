load("@npm//:defs.bzl", "npm_link_all_packages")
load("@npm//web:webpack/package_json.bzl", webpack_bin = "bin")
load("@aspect_rules_js//js:defs.bzl", "js_library")
load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

npm_link_all_packages(
    name = "node_modules",
)

filegroup(
    name = "templates",
    srcs = glob(["templates/**/*"]),
)

filegroup(
    name = "htdocs",
    srcs = glob(["htdocs/**"]),
)

js_library(
    name = "lib",
    srcs = glob([
        "src/**",
    ]),
    deps = [
        ":node_modules/css-loader",
        ":node_modules/style-loader",
        ":node_modules/webpack",
        "//web/3d:html",
    ],
)

webpack_bin.webpack(
    name = "webpack_build",
    srcs = [
        "webpack.config.js",
        ":lib",
    ],
    outs = [
        "htdocs/assets/js/bundle.js",
    ],
    args = [
        "--config",
        "webpack.config.js",
    ],
    chdir = package_name(),
    log_level = "debug",
)

genrule(
    name = "asset_hashes",
    srcs = [
        "//web:htdocs",
        "//web:webpack_build",
    ],
    outs = ["hashes.txt"],
    cmd = """sha256sum $(SRCS) | sed "s# $(BINDIR)/# #" | sed "s# web/htdocs/# #"> $@""",
)

pkg_tar(
    name = "assets",
    srcs = [
        ":asset_hashes",
        ":htdocs",
        ":templates",
        ":webpack_build",
    ],
    package_dir = "web/",
    strip_prefix = ".",
)
