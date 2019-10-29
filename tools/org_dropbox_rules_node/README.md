# Node Rules for Bazel (unsupported)

Rules | Description
--- | ---
[node_binary] | Creates a node binary.
[node_library] | Groups node.js sources and deps together.
[npm_library] | Defines an external npm module.
[node_internal_module] | Create an internal node module that can be required as `require('module_name')`.
[webpack_build] | Build JS/CSS/etc with webpack.
[node_build] | Build JS/CSS/etc with a node binary.
[mocha_test] | Defines a node test that uses mocha.
[node_test] | Defines a basic node test.

## Overview

These rules are a public copy of what we're using at [Dropbox]. We
open sourced them because we think the community will benefit from
seeing how we've done things, but they are not supported. Pull
requests are welcome, though!

We encourage you use the officially sanctioned
[nodejs][bazelbuild-rules_nodejs] or
[typescript][bazelbuild-rules_typescript] rules instead, if possible!

A brief overview:

 - [node_binary], [node_library], [mocha_test], [node_test] all work
   the way you would expect them to.

 - [npm_library] downloads npm modules from the public npm repository. We
   have a fair amount of tooling within Dropbox to support this rule,
   with a private mirror and way to generate `npm_library` rules.
   Simpler versions of those tools are included in
   [node/tools/npm][npm-tooling]. If you're using this rule for
   serious development, you should replace the `npm_installer` with
   something that pulls from an internal mirror.

 - [webpack_build] uses webpack to build js/css files. Making the
   experience of using webpack better within Dropbox was one of the
   reasons we wrote these rules.

 - [node_internal_module] is used to create an "internal" node module
   so that you can easily share code without having to upload it to
   npm.


### Design decisions

 - As much as possible, we try to create a "normal" node environment
   in the runfiles for node binaries. This simplifies debugging (it's
   easier to create test cases without Bazel) and reduces the learning
   curve for node developers.

 - We use the `--preserve-symlinks` so that node doesn't get the
   realpath of the file and look up the node_modules outside of its
   runfiles.

 - The `node_modules` folder is created in the runfiles and is placed
   in the package directory that contains the `node_binary` target.
   This simplifies things because you can have the `main` for a binary
   be inside its `node_modules`. For example:

```bzl
node_binary(
    name = 'webpack_bin',
    main = 'node_modules/webpack/bin/webpack.js',
    deps = ['//npm/webpack'],
)
```

 - The `contents` attr for `npm_library` is a list of strings that are
   turned into files instead of a list of labels because files have
   fewer character restrictions.

 - Compiled node modules are not currently supported.


### Examples

See [examples].


## Setup

First you must [install][bazel-install] Bazel.

### Linux

For Linux, you must add the following to your `WORKSPACE` file:

```bzl
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "org_dropbox_rules_node",
    remote = "https://github.com/dropbox/rules_node.git",
    commit = "{HEAD}",
)

load("@org_dropbox_rules_node//node:defs.bzl", "node_repositories")

node_repositories()
```

This will pull in node v6.11.1 built for linux-x64. If you want to use
another version of node, you should pass `omit_nodejs=True` and define
another version of `nodejs` in your `WORKSPACE` file.

### macOS

NOTE: These rules have only been tested on Linux.

For macOS, you must add the following to your `WORKSPACE` file:

```bzl
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "org_dropbox_rules_node",
    remote = "https://github.com/dropbox/rules_node.git",
    commit = "{HEAD}",
)

load("@org_dropbox_rules_node//node:defs.bzl", "node_repositories", "NODEJS_BUILD_FILE_CONTENT")

node_repositories(omit_nodejs=True)

http_archive(
    name = "nodejs",
    url = "https://nodejs.org/dist/v6.11.1/node-v6.11.1-darwin-x64.tar.gz",
    strip_prefix = "node-v6.11.1-darwin-x64",
    sha256 = "a2b839259089ef26f20c17864ff5ce9cd1a67e841be3d129b38d288b45fe375b",
    build_file_content = NODEJS_BUILD_FILE_CONTENT,
)
```

This will pull in node v6.11.1 built for macOS.

# Rules

## `node_binary`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "node_binary")
node_binary(name, main, srcs, deps, data, extra_args, node, max_old_memory, expose_gc)
```

Creates a node binary, which is an executable Node program consisting
of a collection of `.js` source files.

Node binaries are created using `--preserve-symlinks`.

One quirk of this rule is that if your script uses the pattern:

```javascript
  if (require.main === module) {
```

to only execute something if the node script is the "main" script,
you'll need to modify it to check `BAZEL_NODE_MAIN_ID` against the
module id instead, like this:

```javascript
  if (process.env['BAZEL_NODE_MAIN_ID'] === module.id || require.main === module) {
```

All node modules that this rule depends on, direct and transitive, end
up flattened in `{package-directory}/node_modules`, where
`package-directory` is the directory that the node_binary target
is it. This means that, across all of your transitive dependencies
tracked by Bazel, you can't depend on different versions of the same
node module. (This does not apply to transitive dependencies of
npm_library rules, which are not tracked by Bazel.)

The environmental variable `NODE_PATH` is set to
`{package-directory}/node_modules` so that all the files that end up
running can find all the included node modules.

One side-effect of that is that node libraries have access to all the
transitive dependencies for the node binary that depends on them.

Examples:

```bzl
node_binary(
    name = 'mybin',
    srcs = [
        'mybin.js',
    ],
    main = 'mybin.js',
    deps = [
        '//npm/loose-envify',
        '//npm/minimist',
        '//npm/mkdirp',
    ],
)
```

```bzl
node_library(
    name = 'lib',
    srcs = ['lib.js'],
    deps = [
        '//npm/mkdirp',
    ],
)

node_binary(
    name = 'bin',
    srcs = ['bin.js'],
    deps = [
        ':lib',
    ],
)
```

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **main:** ([Label]; required) The name of the source file that is the main entry point of the
    application.

 - **srcs:** (List of [labels]; optional) The list of source files that are processed to create the target.

 - **deps:** (List of [labels]; optional) The list of other libraries included in the binary target.

 - **data:** (List of [labels]; optional) The list of files needed by this binary at runtime.

 - **extra_args:** (List of strings; optional) Command line arguments that bazel will pass to the target when it
    is executed either by the `run` command or as a test. These arguments
    are passed before the ones that are specified on the `bazel run` or
    `bazel test` command line.

 - **node:** ([Label]; defaults to `@nodejs//:node`) The node binary used to run the binary. Must be greater than 6.2.0.

 - **max_old_memory:** (Integer; optional) Node, by default, doesn't run its garbage collector on old space
    until a maximum old space size limit is reached. This overrides the default (of around 1.4gb)
    with the provided value in MB.

 - **expose_gc:** (Boolean; optional) Expose `global.gc()` in the node process to allow manual requests for
    garbage collection.

## `node_library`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "node_library")
node_library(name, srcs, deps, data)
```

Groups node.js sources and deps together. Similar to [py_library] rules.

**NOTE:** This does not create an internal module that you can then `require`. For that, you need to use [node_internal_module].

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **srcs:** (List of [labels]; optional) The list of source files that are processed to create the target.

 - **deps:** (List of [labels]; optional) The list of other libraries or node modules needed to be linked into
    the target library.

 - **data:** (List of [labels]; optional) The list of files needed by this library at runtime.

## `npm_library`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "npm_library")
npm_library(name, npm_req, no_import_main_test, shrinkwrap, contents,
            npm_installer, npm_installer_args)
```

Defines an external npm module.

This rule should usually be generated using `//node/tools/npm:gen_build_npm`, like this:

```bash
bazel run @org_dropbox_rules_node//node/tools/npm:gen_build_npm -- my-module@1.2.3 ~/myrepo/npm/my-module
```

The module and its dependencies are downloaded using `npm_installer`.

All of the module's dependencies are declared in the shrinkwrap but
aren't known by Bazel. This is to work around Bazel's restrictions on
circular dependencies, which are commonplace in the node ecosystem. By
doing things this way, Bazel will know about your direct `npm`
dependencies, but not your indirect dependencies.

One restriction that this rule places on it's output is that all
the files output must either be in the module's directory or in
'.bin'. E.g. if the module is named `module-name`, then this list
of contents is legal:

    contents = [
        'module-name/src/something.js',
        '.bin/some-binary',
    ]

But this list of contents is not:

    contents = [
        'module-name/src/something.js',
        'another-modules/something-else.js',
    ]

This allows us to be reasonably sure that any two modules can be
used together (except when '.bin' has conflicts, which should be
rare).

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **npm_req:** (String; required) The npm string used to download this module. Must be in the form `module-name@1.2.3`.

 - **no_import_main_test:** (Boolean; defaults to `False`) Don't test that the npm library can be required as if it has a main file.

    We test that all imports can be imported like: `require('module')`. For some imports that don't
    have a `main` js file to execute, this import will fail. For example, `@types/` npm modules will
    fail. Set this to True to disable that check.

    NOTE: This rule will still generate a test to make sure that module version is correct.

 - **shrinkwrap:** (String; required) The shrinkwrap file that lists out all the node modules to install. Should usually be `npm-shrinkwrap.json`.

 - **contents:** (List of strings; required) All of the files included in the module.

    This should usually be autogenerated.

    `contents` is a string list instead of a label list to get around Bazel's restrictions on
    label names, which are violated by npm packages pretty often. Some restrictions still exist,
    like the restriction that file names cannot contain whitespace.

 - **npm_installer:** ([Label]; defaults to `@org_dropbox_rules_node//node/tools/npm:install) The binary to use to install the npm modules.

    The default npm_installer downloads them from the public npm registry, but ideally you
    should replace it with a binary that downloads from your private mirror.

 - **npm_installer_args:** (List of strings; optional) Extra arguments to pass to `npm_installer`.

## `node_internal_module`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "node_internal_module")
node_internal_module(name, srcs, deps, data, require_name, package_json, main)
```

Create an internal node module that can be included in the `deps` for
other rules and that can be required as `require('module_name')`.

The module name used to require the module (i.e.
`require('module_name')`) defaults to the name of the target.

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **srcs:** (List of [labels]; optional) The list of source files that are processed to create the target.

 - **deps:** (List of [labels]; optional) The list of other libraries included in the target.

 - **data:** (List of [labels]; optional) The list of files needed by this target at runtime.

 - **require_name:** (String; optional) The name for the internal node module. E.g.
    if `require_name = "my-module"`, it should be used like
    `require('my-module')`. The default is the target name.

 - **package_json:** (String; optional) The package.json for the project. Cannot be
    specified along with `main`.

 - **main:** (String; optional) Defaults to the `main` field in the package.json or
    `index.js` if that doesn't exist. Cannot be specified along with
    `package_json`.

## `webpack_build`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "webpack_build")
webpack_build(name, srcs, deps, data, outs, extra_args, env, config,
               webpack_target)
```

Build JS/CSS/etc with webpack.

It's recommend that you use the internal node module `dbx-bazel-utils`
(`@org_dropbox_rules_node//node/dbx-bazel-utils`). If you use it like
this:

```javascript
var dbxBazelUtils = require('dbx-bazel-utils');
var env = dbxBazelUtils.initBazelEnv(__dirname);
```

Then it will set the working directory to the directory that contains
webpack.config.js, which is usually what you want, and you should
output to the directory in `env.outputRoot`.

If the webpack build is run outside of Bazel, then `env.outputRoot`
will be `__dirname`.

Defaults to using webpack@3.4.1. You can specify your own webpack
target with `webpack_target`.

Examples:

```bzl
# webpack_build/BUILD
load('@org_dropbox_rules_node//node:defs.bzl', 'webpack_build')

webpack_build(
    name = 'webpack_build',
    srcs = glob([
        'src/*.js',
    ]),
    outs = ['bundle.js'],
    config = 'webpack.config.js',
    deps = [
        '@org_dropbox_rules_node//node/dbx-bazel-utils',
    ],
)
```

```javascript
// webpack_build/webpack.config.js
var path = require('path');
var dbxBazelUtils = require('dbx-bazel-utils');
var env = dbxBazelUtils.initBazelEnv(__dirname);

module.exports = {
  entry: ['entry.ts'],
  output: {
    filename: 'bundle.js',
    path: env.outputRoot,
  },
}
```

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **srcs:** (List of [labels]; optional) The list of source files that are processed to create the target.

 - **deps:** (List of [labels]; optional) The list of other libraries included in the target.

 - **data:** (List of [labels]; optional) The list of files needed by this target at runtime.

 - **outs:** (List of [labels]; required) The list of files output by this rule.

 - **extra_args:** (List of strings; optional) The list of additional args for this build.

 - **env:** (Dict of strings; optional) Additional environmental variables to set for the build.

 - **config:** ([Label]; required) The webpack.config.js file.

 - **webpack_target:** ([Label]; defaults to `@org_dropbox_rules_node//npm/webpack`) The webpack target to use.

## `node_build`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "node_build")
node_build(name, outs, data, builder, extra_args, env, optimize_flag)
```

Build JS/CSS/etc with a node binary.

This is a low-level rule, and is only recommended for completely
custom node builds. If you're using webpack, you should use the
[webpack_build] rule. If you're using another standard JS build system
(rollup, gulp, grunt, ...), you should write a macro that follows the
conventions of [webpack_build].

This rule does not have a `srcs` attribute because it expects all the
srcs needed for the build to be included in the `builder` binary.

The environmental variable `BAZEL_OUTPUT_DIR` is set for all builds.
The `builder` binary should output to that directory.

### Arguments

 - **name:** ([Name]; required) A unique name for this rule.

 - **data:** (List of [labels]; optional) The list of files needed by this target at runtime.

 - **outs:** (List of [labels]; required) The list of files output by this rule.

 - **builder:** ([Label]; required) The node binary used to build.

 - **env:** (Dict of strings; optional) Additional environmental variables to set for the build.

 - **extra_args:** (List of strings; optional) The list of additional args for this build.

 - **optimize_flag:** (String; optional) The flag to pass to the build when it's run in "opt" mode.
    If this flag is not defined, then no flag is passed in "opt" mode.

## `mocha_test`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "mocha_test")
mocha_test(name, srcs, deps, extra_args, mocha_target, chai_target, **kwargs)
```

Defines a node test that uses mocha. Takes the same args as
`node_binary`, except that you can't pass a `main` arg,
because mocha is run as the main js file.

Includes dependencies on mocha@3.4.2 and chai@4.1.0 by default. You
can change those dependencies by setting `mocha_target` and
`chai_target`.

### Arguments

Takes the same arguments as node_binary, except for the following:

 - **mocha_target:** The target to use for including 'mocha'.

 - **chai_target:** The target to use for including 'chai'.

## `node_test`

```bzl
load("@org_dropbox_rules_node//node:defs.bzl", "node_test")
node_test(**kwargs)
```

Defines a basic node test. Succeeds if the program has a return code of
0, otherwise it fails.

### Arguments

Has the same arguments as [node_binary].

[Dropbox]: https://www.dropbox.com
[Name]: http://bazel.io/docs/build-ref.html#name
[labels]: http://bazel.io/docs/build-ref.html#labels
[Label]: http://bazel.io/docs/build-ref.html#labels
[bazel-install]: https://docs.bazel.build/versions/master/install.html
[node_binary]: #node_binary
[node_library]: #node_library
[npm_library]: #npm_library
[node_internal_module]: #node_internal_module
[node_build]: #node_build
[webpack_build]: #webpack_build
[node_test]: #node_test
[mocha_test]: #mocha_test
[py_library]: https://docs.bazel.build/versions/master/be/python.html#py_library
[npm-bazel]: https://github.com/redfin/npm-bazel
[pubref_node]: https://github.com/pubref/rules_node
[bazelbuild]: https://github.com/bazelbuild
[npm-tooling]: https://github.com/dropbox/rules_node/tree/master/node/tools/npm
[examples]: https://github.com/dropbox/rules_node/tree/master/examples
[bazelbuild-rules_nodejs]: https://github.com/bazelbuild/rules_nodejs
[bazelbuild-rules_typescript]: https://github.com/bazelbuild/rules_typescript
