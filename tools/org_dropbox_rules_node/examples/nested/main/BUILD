package(default_visibility = ['//visibility:public'])

load('@org_dropbox_rules_node//node:defs.bzl', 'node_binary')

node_binary(
    name = 'main',
    srcs = ['main.js'],
    main = 'main.js',
    deps = [
        '//examples/nested:imported_by_nested',
    ],
)
