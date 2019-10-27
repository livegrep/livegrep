import argparse
import json
import os
import shutil
from tempfile import mkdtemp

from node.tools.npm.utils import (
    run_npm,
    read_shrinkwrap,
    get_dep_name,
    get_dep_version,
    SHRINKWRAP,
)

BUILD_OUTPUT = 'BUILD'

NPM_RULE_TYPES = ('npm_library', )

LOAD_STATEMENT = """# @generated: Generated with gen_build_npm.py

package(default_visibility = ['//visibility:public'])

load('@org_dropbox_rules_node//node:defs.bzl', %s)
""" % (
    ', '.join([repr(t) for t in NPM_RULE_TYPES]))


class NpmBuildGeneratorException(Exception):
    pass

def encode_npm_name(name):
    '''
    Encode `name` so that it's a valid npm package name, and so that
    we know it was generated using this tool.
    '''
    return 'npm-gen-' + name.replace('/', '-').replace('@', 'at_')

def get_npm_library_contents(target_dir, npm_req):
    ''' Return a list of the contents of the npm module and its dependencies.  '''
    name = get_dep_name(npm_req)
    version = get_dep_version(npm_req)

    # Generate the npm-shrinkwrap.json if its name and version
    # don't match the shrinkwrap that already exists, or if the
    # shrinkwrap doesn't exist.
    shrinkwrap_is_valid = False
    repo_shrinkwrap = os.path.join(
        target_dir,
        SHRINKWRAP)

    if os.path.isfile(repo_shrinkwrap):
        try:
            repo_shrinkwrap_data = read_shrinkwrap(repo_shrinkwrap)
        except Exception:
            # Current shrinkwrap is in a bad state, ignore it.
            repo_shrinkwrap_data = None

        # True if the name and version match.
        shrinkwrap_is_valid = repo_shrinkwrap_data and \
                              (repo_shrinkwrap_data['name'] == encode_npm_name(name)) and \
                              (repo_shrinkwrap_data['version'] == version)

    tmpdir = mkdtemp(suffix='gen_build_npm')

    def run_npm_in_tmpdir(cmd):
        env = {
            # Change the npm cache. This isn't required, but
            # avoids cases where the user's default npm cache is
            # in a bad state (which can happen with npm).
            'NPM_CONFIG_CACHE': os.path.join(tmpdir, '.npm'),
        }
        run_npm(cmd, env=env, cwd=tmpdir)

    if shrinkwrap_is_valid:
        # Copy the shrinkwrap and do an npm install from it.
        shutil.copyfile(repo_shrinkwrap, os.path.join(tmpdir, SHRINKWRAP))

        run_npm_in_tmpdir(['install'])
    else:
        # We need to generate a shrinkwrap file first.
        # First, generate our package.json
        package_json = os.path.join(tmpdir, 'package.json')
        with open(package_json, 'w') as f:
            json.dump({
                # The only required field is 'dependencies', but
                # we add name and version because we want them to
                # end up in the generated shrinkwrap.
                'name': encode_npm_name(name),
                'version': version,
                'dependencies': {
                    name: version,
                },
            }, f)

        # Then we need to do an `npm install`, because you need to
        # install your dependencies before creating the
        # shrinkwrap.
        run_npm_in_tmpdir(['install'])

        # Remove peer dependencies from the package.json. This is
        # needed because `npm shrinkwrap` will fail if the peer
        # dependencies don't exist, even though we don't want to
        # package the peer dependencies along with the module.
        #
        # It's up to the caller to include peer dependencies when
        # they import the module.
        module_package_json = os.path.join(
            tmpdir, 'node_modules', name, 'package.json')
        with open(module_package_json, 'r') as f:
            module_package_data = json.load(f)

        # Walk the module's directory looking for all the package.json
        # files, so we also remove the peer dependencies of the package's dependencies.
        for root, dirs, files in os.walk(os.path.join(tmpdir, 'node_modules', name)):
            for fname in files:
                if fname == 'package.json':
                    package_json_path = os.path.join(root, fname)

                    with open(package_json_path, 'r') as f:
                        module_package_data = json.load(f)

                    if 'peerDependencies' in module_package_data:
                        del module_package_data['peerDependencies']
                        with open(package_json_path, 'w') as f:
                            json.dump(module_package_data, f)

        # Finally, create the npm shrinkwrap file and then move it
        # back to the repo.
        run_npm_in_tmpdir(['shrinkwrap'])
        # A 'move' operation is technically better, but may fail
        # if /tmp is on a different partition, which is true in
        # EC.
        shutil.copyfile(os.path.join(tmpdir, SHRINKWRAP), repo_shrinkwrap)

    # At this point, the tmpdir has all of the node modules
    # installed to node_modules. Now we need to collect all of the
    # files in node_modules.

    # node_module_dir ends with a '/'.
    node_modules_dir = os.path.join(tmpdir, 'node_modules', '')
    assert os.path.isdir(node_modules_dir)
    contents = []
    for root, dirs, files in os.walk(node_modules_dir):
        relative_root = root[len(node_modules_dir):]
        if relative_root:
            relative_root += '/'

        for filename in files:
            # Spaces are forbidden in bazel runfiles. Most of the
            # time, these are just in test files so they're okay
            # to exclude.
            if " " in filename:
                print "WARNING: Not including file %s because it contains whitespace. " \
                    "This is usually safe because these files are usually only for " \
                    "tests. Target: //%s " % (relative_root + filename, target_dir)
                continue

            contents.append(relative_root + filename)

    # Clean up tmpdir so we don't leave a bunch of tmpdirs behind.
    shutil.rmtree(tmpdir)

    contents.sort()
    return contents

def create_build_file(target_dir, npm_rule, contents):
    output = [LOAD_STATEMENT]

    attr_map = npm_rule.attr_map

    # Generate the npm library rule.
    output.append('%s(' % npm_rule.rule_type)
    output.append('    name = %s,' % repr(attr_map['name']))
    output.append('    npm_req = %s,' % repr(attr_map['npm_req']))
    output.append('    shrinkwrap = %s,' % repr(attr_map['shrinkwrap']))
    output.append('    contents = [')
    for content in contents:
        output.append('        %s,' % repr(content))
    output.append('    ],')
    output.append(')')

    build_output = os.path.join(target_dir, BUILD_OUTPUT)
    with open(build_output, 'w') as f:
        f.write('\n'.join(output))


class BazelRule(object):
    def __init__(self, attr_map, rule_type):
        self.attr_map = attr_map
        self.rule_type = rule_type


def generate_build_file_and_shrinkwrap(npm_req, output):
    '''
    Generate the BUILD file and potentially generate a new shrinkwrap file.

    We don't generate a new shrinkwrap every time because `npm
    shrinkwrap` is non-deterministic. A new shrinkwrap is only
    generated if the npm module in the shrinkwrap is not the
    version specified in the target rule.
    '''
    if not os.path.exists(output):
        os.makedirs(output)

    contents = get_npm_library_contents(output, npm_req)

    name = get_dep_name(npm_req)

    npm_rule = BazelRule(
        attr_map = {
            'name': name,
            'contents': contents,
            'npm_req': npm_req,
            'shrinkwrap': SHRINKWRAP,
        },
        rule_type = 'npm_library',
    )

    create_build_file(output, npm_rule, contents)


def main():
    parser = argparse.ArgumentParser(
        description="Generate BUILD and shrinkwrap files for npm modules",
    )
    parser.add_argument("npm_req",
                        help="npm_req for the module you want to add (e.g. `module@1.2.3`)",
                        type=str)
    parser.add_argument("output",
                        help="""Directory to put the BUILD file in. NOTE: You should pass
                        an absolute path (e.g ~/path/to/dir) if you're calling this tool
                        using `bazel run`, because `bazel run` sets the working directory to
                        the runfiles dir.""",
                        type=str)

    args = parser.parse_args()

    generate_build_file_and_shrinkwrap(args.npm_req, args.output)

if __name__ == '__main__':
    main()
