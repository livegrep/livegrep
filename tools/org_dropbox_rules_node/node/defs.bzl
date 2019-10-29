"""Node Rules

Bazel rules for working with node.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Helper functions

runfiles_tmpl = '''#!/bin/bash -eu
# If a binary is listed as a data dep for another tool, you
# need to scan a bit differently for the runfiles dir. Of course, once
# this is resolved by realpath, the path will never have .runfiles in
# it when developing because of the way the files are symlinked, hence
# splitting the abspath.
#
# This also allows symlinks to bazel built binaries in production to correctly
# find their runfiles. This is commonly used in production to put bazel
# binaries in the PATH.
#
# Note: We call python by passing the script through stdin instead of using -c
#       to prevent Python from prepending '' to sys.path and allowing
#       modules from $PWD to be imported as top level modules.

STUBPATH=$(/usr/bin/env python -ESs <(echo "import os.path; print(os.path.realpath(os.path.abspath('$0').split('.runfiles')[0]));"))
STUBPATH=$STUBPATH.runfiles

export RUNFILES=$STUBPATH/{workspace_name}

{content}
'''

def _get_runfiles_tmpl(ctx, content):
    # If we're building an external binary, then we need to use the
    # workspace name of that binary, not the current workspace.
    if ctx.label.workspace_root:
        if not ctx.label.workspace_root.startswith("external/"):
            fail("Workspace root must start with external/: {}".format(
                ctx.label.workspace_root,
            ))
        workspace_name = ctx.label.workspace_root[len("external/"):]
    else:
        workspace_name = ctx.workspace_name

    return runfiles_tmpl.format(
        content = content,
        workspace_name = workspace_name,
    )

def _get_package_dir(ctx):
    return ctx.label.package

def _get_output_dir(ctx):
    # If it's an external label, output to workspace_root.
    if ctx.label.workspace_root:
        return ctx.configuration.bin_dir.path + "/" + ctx.label.workspace_root + "/" + _get_package_dir(ctx)

    return ctx.configuration.bin_dir.path + "/" + _get_package_dir(ctx)

def _get_relpath(ctx, src):
    package_prefix = _get_package_dir(ctx) + "/"

    relpath = src.short_path

    # If relpath starts with '../', that means it belongs to an
    # external dependency, and it'll look like:
    #   '../{workspace_name}/{package_path}/{file}
    # We want to transform it so it includes the package path
    # and the file.
    if relpath.startswith("../"):
        parts = relpath.split("/")
        relpath = "/".join(parts[2:])

    if not relpath.startswith(package_prefix):
        fail("Path must be in the package: (path: {relpath}, package prefix: {package_prefix})".format(
            relpath = relpath,
            package_prefix = package_prefix,
        ))

    return relpath[len(package_prefix):]

def _new_node_modules_srcs_dict():
    """Returns a new `node_modules_srcs_dict` dict. It's just a normal
    dict, this function exists so we can document how it works in one
    place.

    `node_modules_srcs_dict` is a map from paths relative to the
    highest-level node_modules directory to Files.

    E.g. if you have a file that's going to end up in:
        main-module/node_modules/some-dep/dep.js

    The key should be `some-dep/dep.js`.

    NOTE: Make sure to fail if you need to add a path to
    `node_modules_srcs_dict` that already exists.

    """
    return {}

def _new_all_node_modules():
    """Returns a new `all_node_modules` dict. It's just a normal
    dict, this function exists so we can document how it works in one
    place.

    `all_node_modules` is a map from unique targets to
    `node_modules_srcs_dict`s.

    The key should be unique to the package, and doesn't necessarily
    have any other semantic meaning.
    """
    return {}

def _npm_library_impl(ctx):
    node_module_srcs = []

    npm_req = ctx.attr.npm_req
    if "@" not in npm_req:
        fail("npm_req must contain a '@': {npm_req}".format(npm_req = npm_req))

    # The module name is everything before the last '@'.
    require_name = npm_req.rsplit("@", 1)[0]
    require_prefix = require_name + "/"

    node_modules_srcs_dict = _new_node_modules_srcs_dict()
    for content in ctx.attr.contents:
        if content in node_modules_srcs_dict:
            fail("Duplicate path in node_modules: {content}".format(content = content))

        if not content.startswith(require_prefix) and not content.startswith(".bin/"):
            fail(("npm library cannot include paths outside of {require_name} " +
                  "or '.bin': {content}").format(require_name = require_name, content = content))

        # The created files are all under `node_modules/` because
        # that's where the npm installer puts them.
        node_modules_srcs_dict[content] = ctx.actions.declare_file("node_modules/" + content)

    shrinkwrap = ctx.file.shrinkwrap

    command_args = [
        shrinkwrap.path,
        _get_output_dir(ctx),
    ]
    if ctx.attr.npm_installer_extra_args:
        command_args.extend(ctx.attr.npm_installer_extra_args)

    env = {}
    if "HTTP_PROXY" in ctx.var:
        env["HTTP_PROXY"] = ctx.var["HTTP_PROXY"]
    if "HTTPS_PROXY" in ctx.var:
        env["HTTPS_PROXY"] = ctx.var["HTTPS_PROXY"]

    ctx.actions.run(
        inputs = [shrinkwrap],
        outputs = node_modules_srcs_dict.values(),
        executable = ctx.executable.npm_installer,
        arguments = command_args,
        progress_message = "installing node modules from {}".format(shrinkwrap.path),
        mnemonic = "InstallNPMModules",
        env = env,
    )

    all_node_modules = _new_all_node_modules()
    all_node_modules[str(ctx.label)] = node_modules_srcs_dict

    return struct(
        all_node_modules = all_node_modules,
    )

_npm_library_internal = rule(
    attrs = {
        "shrinkwrap": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "npm_req": attr.string(mandatory = True),
        "contents": attr.string_list(mandatory = True),
        "npm_installer": attr.label(
            executable = True,
            cfg = "host",
        ),
        "npm_installer_extra_args": attr.string_list(),
    },
    implementation = _npm_library_impl,
)

def npm_library(
        name,
        npm_req,
        shrinkwrap,
        contents,
        no_import_main_test = False,
        npm_installer = "@org_dropbox_rules_node//node/tools/npm:install",
        npm_installer_extra_args = []):
    """Defines an external npm module.

    This rule should usually be generated using
    `//node/tools/npm:gen_build_npm`, like this:

      bazel run @org_dropbox_rules_node//node/tools/npm:gen_build_npm -- my-module@1.2.3 ~/src/myrepo/npm/my-module

    Defines an external npm module. The module and its dependencies
    are downloaded using `npm_installer`.

    All of the module's dependencies are declared in the shrinkwrap but
    aren't known by Bazel. This is to work around Bazel's restrictions on
    circular dependencies.

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

    Args:
      npm_req: The npm string used to download this module. Must be in the form `module-name@1.2.3`.
        Used to auto-generate this rule.

      no_import_main_test: Don't test that the npm library can be required as if it has a main file. Defaults to False.

        We test that all imports can be imported like: `require('module')`. For some imports that don't
        have a `main` js file to execute, this import will fail. For example, `@types/` npm modules will
        fail. Set this to True to disable that check.

        NOTE: This rule will still generate a test to make sure that module version is correct.

      shrinkwrap: The shrinkwrap file that lists out all the node modules to install.

      contents: All of the files needed by the module.

        contents is a string list instead of a label list to get around Bazel's restrictions on
        label names, which are violated by npm packages pretty often. Some restrictions still exist,
        like the restriction that file names cannot contain whitespace.

      npm_installer: The binary to use to install the npm modules.

        The default npm_installer downloads them from the public npm registry, but ideally you
        should replace it with a binary that downloads from your private mirror.

      npm_installer_args: Extra arguments to pass to `npm_installer`.

    """
    _npm_library_internal(
        name = name,
        contents = contents,
        npm_installer = npm_installer,
        npm_installer_extra_args = npm_installer_extra_args,
        npm_req = npm_req,
        shrinkwrap = shrinkwrap,
    )
    _node_import_test(
        name = name + "_import_test",
        no_import_main_test = no_import_main_test,
        npm_req = npm_req,
        deps = [name],
    )

def _collect_srcs_and_deps(ctx):
    """
    Collects all the srcs and deps from ctx's direct and transitive dependencies.

    Returns the tuple (srcs, all_node_modules).
    """
    srcs = depset(ctx.files.srcs)
    all_node_modules = _new_all_node_modules()

    # Verify that `srcs` are not node_library or npm_library rules.
    for src in ctx.attr.srcs:
        if hasattr(src, "all_node_modules"):
            fail("Invalid src, srcs must only be files: {}".format(src.label))

    for dep in ctx.attr.deps:
        if hasattr(dep, "srcs"):
            srcs += dep.srcs

        if hasattr(dep, "all_node_modules"):
            for key, value in dep.all_node_modules.items():
                if key not in all_node_modules:
                    all_node_modules[key] = value

    return (srcs, all_node_modules)

def _node_library_impl(ctx):
    srcs, all_node_modules = _collect_srcs_and_deps(ctx)

    return struct(
        srcs = srcs,
        all_node_modules = all_node_modules,
        runfiles = ctx.runfiles(collect_default = True),
    )

node_library = rule(
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "deps": attr.label_list(allow_files = False),
        "data": attr.label_list(allow_files = True),
    },
    implementation = _node_library_impl,
)

"""
Groups node.js sources and deps together.

Args:
  srcs: The list of source files that are processed to create the target.
  deps: The list of other libraries or node modules needed to be linked into
    the target library.
  data: The list of files needed by this library at runtime.
"""

def _construct_runfiles(ctx, srcs, all_node_modules):
    """
    Create the set of runfiles for node rules.

    Puts all of the node modules in `$RUNFILES/{pkg_dir}/node_modules`
    for the binary target.

    Fails if any files in all_node_modules conflict with each other.
    """
    package_dir = _get_package_dir(ctx)
    if ctx.label.workspace_root:
        if not ctx.label.workspace_root.startswith("external/"):
            fail("Workspace root must start with external/: {}".format(
                ctx.label.workspace_root,
            ))
        package_dir = ctx.label.workspace_root[len("external/"):] + "/" + package_dir

    symlinks = {}
    for node_modules_srcs_dict in all_node_modules.values():
        for content_path, src in node_modules_srcs_dict.items():
            path = package_dir + "/node_modules/" + content_path

            if path in symlinks:
                fail(("Cannot have conflicting paths in node_modules. This path was " +
                      "in your node modules twice: {path}. These are the deps included " +
                      "in your node_modules folder: {all_node_modules_keys}").format(
                    path = path,
                    all_node_modules_keys = all_node_modules.keys(),
                ))

            symlinks[path] = src

    # If the binary is in an external workspace, then we should create
    # symlinks relative to the runfiles root.
    if ctx.label.workspace_root:
        return ctx.runfiles(
            files = list(srcs),
            root_symlinks = symlinks,
            collect_default = True,
        )
    else:
        return ctx.runfiles(
            files = list(srcs),
            symlinks = symlinks,
            collect_default = True,
        )

def _node_binary_impl(ctx):
    srcs, all_node_modules = _collect_srcs_and_deps(ctx)

    # Add node binary to runfiles
    srcs = srcs.to_list()
    srcs += [ctx.file.node]

    node_flags = ["--preserve-symlinks"]
    if ctx.attr.max_old_memory:
        node_flags.append("--max_old_space_size={}".format(ctx.attr.max_old_memory))
    if ctx.attr.expose_gc:
        node_flags.append("--expose-gc")

    default_args = " ".join(ctx.attr.extra_args + ['"$@"'])

    # We use --preserve-symlinks so that node will respect the symlink
    # tree in runfiles. One quirk of preserve symlinks is that
    # symlinks aren't preserved for the "main" module. To work around
    # this, we create an wrapper js file that just has a call to
    # `require('/path/to/main.js')`.
    #
    # We can't do `node --eval "require('/path/to/main.js')"` because
    # node won't populate process.argv correctly.

    # Create the inner wrapper with the require call.
    inner_wrapper = ctx.actions.declare_file(ctx.outputs.executable.basename + "-wrapper.js", sibling = ctx.outputs.executable)
    srcs += [inner_wrapper]
    ctx.actions.write(
        inner_wrapper,
        """require(process.env['BAZEL_NODE_MAIN_ID']);""",
    )

    # Some scripts use the pattern `if (require.main === module) {` to
    # only execute something if the node script is the "main" script.
    # If you want that to work with bazel, you should check
    # BAZEL_NODE_MAIN_ID against the module id instead, like this:
    #   if (process.env['BAZEL_NODE_MAIN_ID'] === module.id || require.main === module) {
    node_runfile_tmpl = """
export BAZEL_NODE_MAIN_ID=$RUNFILES/{main}
export NODE_PATH=$RUNFILES/{node_path}

$RUNFILES/{node} {node_flags} $RUNFILES/{inner_wrapper} {default_args}
"""
    runfile_content = _get_runfiles_tmpl(
        content = node_runfile_tmpl.format(
            main = ctx.file.main.short_path,
            node = ctx.file.node.short_path,
            node_flags = " ".join(node_flags),
            inner_wrapper = inner_wrapper.short_path,
            default_args = default_args,
            node_path = _get_package_dir(ctx) + "/node_modules",
        ),
        ctx = ctx,
    )

    # Generate output executable
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = runfile_content,
        is_executable = True,
    )

    runfiles = _construct_runfiles(ctx, srcs, all_node_modules)
    return struct(runfiles = runfiles)

_node_bin_attrs = {
    "srcs": attr.label_list(allow_files = True),
    "deps": attr.label_list(allow_files = False),
    "data": attr.label_list(allow_files = True),
    "main": attr.label(
        allow_single_file = True,
        mandatory = True,
    ),
    "extra_args": attr.string_list(),
    "node": attr.label(
        allow_single_file = True,
        default = Label("@nodejs//:node"),
    ),
    "max_old_memory": attr.int(),
    "expose_gc": attr.bool(default = False),
}

node_binary = rule(
    attrs = _node_bin_attrs,
    executable = True,
    implementation = _node_binary_impl,
)

"""Creates a node binary, which is an executable Node program consisting
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
    ```python
    node_binary(
        name = 'mybin',
        srcs = [
            'mybin.js',
        ],
        expose_gc = True,
        main = 'mybin.js',
        deps = [
            '//npm/loose-envify',
            '//npm/minimist',
            '//npm/mkdirp',
        ],
    )```

    ```python
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

Args:
  srcs: The list of source files that are processed to create the target.
  deps: The list of other libraries included in the binary target.
  data: The list of files needed by this binary at runtime.
  main: The name of the source file that is the main entry point of the
    application.
  extra_args: Command line arguments that bazel will pass to the target when it
    is executed either by the `run` command or as a test. These arguments
    are passed before the ones that are specified on the `bazel run` or
    `bazel test` command line.
  node: The node binary used to run the binary. Must be greater than 6.2.0.
  max_old_memory: Optional. Node, by default, doesn't run its garbage collector on old space
    until a maximum old space size limit is reached. This overrides the default (of around 1.4gb)
    with the provided value in MB.
  expose_gc: Optional. Expose `global.gc()` in the node process to allow manual requests for
    garbage collection.

"""

node_test = rule(
    attrs = _node_bin_attrs,
    test = True,
    implementation = _node_binary_impl,
)

"""
Defines a basic node test. Succeeds if the program has a return code of
0, otherwise it fails.

Args:
  srcs: The list of source files that are processed to create the target.
  deps: The list of other libraries included in the binary target.
  data: The list of files needed by this binary at runtime.
  main: The name of the source file that is the main entry point of the
    application.
  extra_args: Command line arguments that bazel will pass to the target when it
    is executed either by the `run` command or as a test. These arguments
    are passed before the ones that are specified on the `bazel run` or
    `bazel test` command line.
  node: The node binary used to run the binary. Must be greater than 6.2.0.
"""

def mocha_test(
        name,
        srcs = [],
        deps = [],
        extra_args = [],
        mocha_target = "@org_dropbox_rules_node//npm/mocha",
        chai_target = "@org_dropbox_rules_node//npm/chai",
        **kwargs):
    """
    Defines a node test that uses mocha. Takes the same args as
    `node_binary`, except that you can't pass a `main` arg,
    because mocha is run as the main js file.

    Args:
      srcs: The list of files to test.
      deps: The list of other libraries included in the binary target.
      extra_args: The list of other args for this test.
      mocha_target: The target to use for including 'mocha'.
      chai_target: The target ot use for including 'chai'.
    """

    if "main" in kwargs:
        fail("You cannot provide a 'main' to node_test rules. Use 'srcs' instead.")
    if not srcs:
        fail("'srcs' cannot be empty")

    root = "$RUNFILES/" + native.package_name()

    mocha_args = [root + "/" + src for src in srcs]

    additional_deps = [mocha_target, chai_target]

    # bin/mocha is the outer wrapper, and it calls `_mocha` as a
    # new process, which messes up the `require` logic somehow.
    #
    # TODO: After node is upgraded to 7.1.0+, set the env
    # NODE_PRESERVE_SYMLINKS=1 and see if `bin/mocha` works.
    main = "node_modules/mocha/bin/_mocha"

    node_test(
        name = name,
        srcs = srcs,
        main = main,
        deps = depset(additional_deps) + deps,
        extra_args = extra_args + mocha_args,
        **kwargs
    )

def _node_internal_module_impl(ctx):
    if ctx.attr.package_json and ctx.attr.main:
        fail("Can only specify one of package_json or main")

    srcs, all_node_modules = _collect_srcs_and_deps(ctx)

    require_name = None
    if ctx.attr.require_name:
        require_name = ctx.attr.require_name
    else:
        require_name = ctx.label.name

    if ctx.attr.package_json:
        srcs = depset([ctx.file.package_json], transitive = srcs)
    elif ctx.attr.main:
        # Create a package.json file that points to the 'main' js
        # file.
        package_json_src = ctx.actions.declare_file("package.json")
        main = ctx.file.main

        main_relpath = _get_relpath(ctx, main)
        ctx.actions.write(
            package_json_src,
            '''{{"main": "{main_relpath}"}}'''.format(main_relpath = main_relpath),
        )
        srcs = depset([package_json_src], transitive = [srcs])

    node_modules_srcs_dict = _new_node_modules_srcs_dict()
    for src in srcs.to_list():
        relpath = _get_relpath(ctx, src)

        require_path = require_name + "/" + relpath
        if require_path in node_modules_srcs_dict:
            fail("Duplicate path in node_modules: {require_path}".format(
                require_path = require_path,
            ))
        node_modules_srcs_dict[require_path] = src

    all_node_modules[str(ctx.label)] = node_modules_srcs_dict

    return struct(
        all_node_modules = all_node_modules,
        runfiles = ctx.runfiles(collect_default = True),
    )

node_internal_module = rule(
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "deps": attr.label_list(allow_files = False),
        "data": attr.label_list(allow_files = True),
        "require_name": attr.string(),
        "package_json": attr.label(allow_single_file = True),
        "main": attr.label(allow_single_file = True),
    },
    implementation = _node_internal_module_impl,
)

"""
Create an internal node module that can be included in the `deps` for
other rules and that can be required as `require('module_name')`.

Args:
  srcs: The list of source files that are processed to create the target.
  deps: The list of other libraries included in the target.
  data: The list of files needed by this target at runtime.

  require_name: Optional. The name for the internal node module. E.g.
    if `require_name = "my-module"`, it should be used like
    `require('my-module')`. The default is the target name.

  package_json: Optional. The package.json for the project. Cannot be
    specified along with `main`.

  main: Optional. Defaults to the `main` field in the package.json or
    `index.js` if that doesn't exist. Cannot be specified along with
    `package_json`.

"""

def _node_import_test(name, deps, npm_req, no_import_main_test):
    """
    Test that the module `npm_req` can be imported and that its
    version is correct.

    Args:
      deps: The list of other libraries included in the target.
      npm_req: Must be in the form `module-name@1.2.3`.
      no_import_main_test: Don't test that the npm library can be required as if it has a main file.
        See npm_library for details.

    """

    # Break npm_req into its name and version.
    if "@" not in npm_req:
        fail("npm_req is malformed, it must look like `module-name@1.2.3`: " + npm_req)
    split = npm_req.split("@")

    import_name = "@".join(split[:-1])
    import_version = split[-1]

    args = [
        "--import_name=" + import_name,
        "--import_version=" + import_version,
    ]
    if no_import_main_test:
        args.append("--no_import_main_test")

    node_test(
        name = name,
        size = "small",
        srcs = ["@org_dropbox_rules_node//node/tools:import_check.js"],
        extra_args = args,
        main = "@org_dropbox_rules_node//node/tools:import_check.js",
        deps = deps,
    )

def _node_build_impl(ctx):
    env = dict(ctx.attr.env.items() + {
        "BAZEL_OUTPUT_DIR": _get_output_dir(ctx),
    }.items())

    args = []
    if ctx.var["COMPILATION_MODE"] == "opt" and ctx.attr.optimize_flag:
        args.append(ctx.attr.optimize_flag)

    ctx.actions.run(
        executable = ctx.executable.builder,
        outputs = ctx.outputs.outs,
        env = env,
        arguments = args + ctx.attr.extra_args,
        mnemonic = "NodeBuild",
        progress_message = "building {} with node".format(ctx.label.name),
    )
    return struct(
        files = depset(ctx.outputs.outs),
        runfiles = ctx.runfiles(
            files = ctx.outputs.outs,
            collect_default = True,
        ),
    )

node_build = rule(
    attrs = {
        "outs": attr.output_list(mandatory = True),
        "data": attr.label_list(allow_files = True),
        "builder": attr.label(
            executable = True,
            cfg = "host",
            mandatory = True,
        ),
        "env": attr.string_dict(),
        "extra_args": attr.string_list(),
        "optimize_flag": attr.string(),
    },
    implementation = _node_build_impl,
)

"""Build JS/CSS/etc with a node binary.

This is a low-level rule, and is only recommended for completely
custom node builds. If you're using webpack, you should use the
webpack_build rule. If you're using another standard JS build system
(rollup, gulp, grunt, ...), you should write a macro that follows the
conventions of webpack_build.

This rule does not have a `srcs` attribute because it expects all the
srcs needed for the build to be included in the `builder` binary.

The environmental variable `BAZEL_OUTPUT_DIR` is set for all builds.
The `builder` binary should output to that directory.

For more specific use cases, you should write a macro that creates
this rule (see webpack_build).

Args:
  outs: The list of files output by this rule.
  data: The list of files needed by this target at runtime.
  builder: The node binary used to build.
  env: Additional environmental variables to set for the build.
  extra_args: The list of additional args for this build.
  optimize_flag: The flag to pass to the build when it's run in "opt" mode.
    If this flag is not defined, then no flag is passed in "opt" mode.

"""

def webpack_build(
        name,
        srcs = [],
        deps = [],
        data = [],
        config = "",
        outs = [],
        env = {},
        extra_args = [],
        webpack_target = "@org_dropbox_rules_node//npm/webpack"):
    """
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
      ```python
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

    Args:
      srcs: The list of source files that are processed to create the target.
      deps: The list of other libraries included in the target.
      data: The list of files needed by this target at runtime.
      outs: The list of files output by this rule.
      extra_args: The list of additional args for this build.
      env: Additional environmental variables to set for the build.
      config: The webpack.config.js file.
      webpack_target: The webpack target to use.

    """
    if not config:
        fail("Must specify a config")
    if not outs:
        fail("Must specify output")

    srcs += [config]

    deps += [webpack_target]

    node_binary(
        name = name + "_bin",
        srcs = srcs,
        data = data,
        extra_args = [
            "--config",
            "$RUNFILES/{pkg}/{config}".format(
                pkg = native.package_name(),
                config = config,
            ),
        ] + extra_args,
        main = "node_modules/webpack/bin/webpack.js",
        deps = deps,
    )
    node_build(
        name = name,
        outs = outs,
        builder = name + "_bin",
        data = data,
        env = env,
        optimize_flag = "-p",
    )

NODEJS_BUILD_FILE_CONTENT = r"""
package(default_visibility = [ "//visibility:public" ])

filegroup(
    name = "node",
    srcs = [ "bin/node" ],
)

filegroup(
    name = "npm",
    srcs = glob([
        "bin/npm",
        "bin/node",
        "lib/node_modules/npm/**/*",
    ]),
)
"""

def node_repositories(omit_nodejs = False):
    if not omit_nodejs:
        http_archive(
            name = "nodejs",
            build_file_content = NODEJS_BUILD_FILE_CONTENT,
            sha256 = "e68cc956f0ca5c54e7f3016d639baf987f6f9de688bb7b31339ab7561af88f41",
            strip_prefix = "node-v6.11.1-linux-x64",
            type = "tar.xz",
            url = "https://nodejs.org/dist/v6.11.1/node-v6.11.1-linux-x64.tar.xz",
        )
