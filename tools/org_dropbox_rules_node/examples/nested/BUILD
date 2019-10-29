package(default_visibility = ['//visibility:public'])

load('@org_dropbox_rules_node//node:defs.bzl', 'node_library')

node_library(
    name = 'imported_by_nested',
    srcs = ['imported_by_nested.js'],
    deps = [
        '//examples/internal_module',
    ],
)
