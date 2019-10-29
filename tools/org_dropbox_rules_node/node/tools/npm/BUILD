package(default_visibility = ['//visibility:public'])

py_library(
    name = 'utils',
    srcs = ['utils.py', 'runfiles.py'],
    data = [
        '@nodejs//:node',
        '@nodejs//:npm',
    ],
)

py_binary(
    name = 'gen_build_npm',
    srcs = [
        'gen_build_npm.py',
    ],
    main = 'gen_build_npm.py',
    deps = [':utils'],
)

py_binary(
    name = 'install',
    srcs = ['install.py'],
    main = 'install.py',
    deps = [':utils'],
)
