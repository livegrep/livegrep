package(default_visibility = ['//visibility:public'])

load('@org_dropbox_rules_node//node:defs.bzl', 'webpack_build')

filegroup(
    name = 'templates',
    srcs = glob(['templates/*']),
)

filegroup(
    name = 'htdocs',
    srcs = glob(['htdocs/**']),
)

webpack_build(
    name = 'webpack_build',
    srcs = glob([
        'src/**',
    ]),
    outs = [
        'htdocs/assets/js/bundle.js',
    ],
    config = 'webpack.config.js',
    deps = [
        '@org_dropbox_rules_node//node/dbx-bazel-utils',
        '//web/npm/css-loader',
        '//web/npm/style-loader',

        '//web/3d:html',
        '//web/npm/highlight.js',
    ],
)
