import os
import sys
import subprocess

from node.tools.npm.runfiles import data_path

SHRINKWRAP = 'npm-shrinkwrap.json'

PUBLIC_NPM_REGISTRY_URL = "https://registry.npmjs.org"
NODE_BIN_PATH = data_path('@nodejs//bin')
NPM_PATH = data_path('@nodejs//bin/npm')


class NodeToolsException(Exception):
    pass

def get_dep_id(dep_name, dep_version):
    return '%s@%s' % (dep_name, dep_version)

def get_dep_name(dep_id):
    assert '@' in dep_id
    return dep_id.rsplit('@', 1)[0]

def get_dep_version(dep_id):
    assert '@' in dep_id
    return dep_id.split('@')[-1]

def read_shrinkwrap(filename):
    with open(filename, 'r') as f:
        try:
            s = json.load(f)
        except ValueError as e:
            raise NodeToolsException("Could not read shrinkwrap %s: %s" % (filename, e))
        return s

def get_npm_registry_url(module_id):
    # type: (str) -> str
    '''
    Returns the public npm registry url for module_id. npm urls look like this:
      https://registry.npmjs.org/rollup/-/rollup-0.41.5.tgz
    or this:
      https://registry.npmjs.org/@types/npm/-/npm-2.0.28.tgz
    '''

    dep_name = get_dep_name(module_id)
    dep_name_last_part_only = dep_name.split('/')[-1]
    dep_version = get_dep_version(module_id)

    return '{npm_registry_url}/{dep_name}/-/{dep_name_last_part_only}-{dep_version}.tgz'.format(
        npm_registry_url=PUBLIC_NPM_REGISTRY_URL,
        dep_name=dep_name,
        dep_name_last_part_only=dep_name_last_part_only,
        dep_version=dep_version,
    )

def run_npm(cmd, env=None, cwd=None):
    '''
    Runs `npm`.

    Args:
      cmd: Additional args to npm. E.g. `['install']`.
      env: Additional env vars. Node is already added to the path.
      cwd: Set the working directory.
    '''
    full_cmd = [NPM_PATH] + cmd

    full_env = {
        # Add node and npm to the path.
        'PATH': NODE_BIN_PATH + ":/usr/bin:/bin",
        # Ignore scripts because we don't want to compile
        # native node modules or do anything weird.
        'NPM_CONFIG_IGNORE_SCRIPTS': 'true',
        # Only direct dependencies will show up in the first
        # `node_modules` level, all transitive dependencies
        # will be flattened starting at the second level.
        #
        # This is needed because, when we combine different
        # npm_library targets, we don't want any potential
        # transitive dependencies to overlap.
        'NPM_CONFIG_GLOBAL_STYLE': 'true',
    }

    if env:
        full_env.update(env)

    if 'HTTP_PROXY' in os.environ:
        full_env['HTTP_PROXY'] = os.environ['HTTP_PROXY']
    if 'HTTPS_PROXY' in os.environ:
        full_env['HTTPS_PROXY'] = os.environ['HTTPS_PROXY']

    try:
        ret = subprocess.check_output(
            full_cmd, env=full_env, cwd=cwd, stderr=subprocess.STDOUT
        ).strip()
    except subprocess.CalledProcessError as e:
        sys.stderr.write(str(e.output))
        raise
    return ret
