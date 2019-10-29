# Install a set of npm modules.
#
# This file installs the npm modules directly from the public npm
# registry. Ideally, you should be using your own installer that pulls
# from an internal mirror of npm.

import argparse
import os
import shutil
from tempfile import mkdtemp
import json

from node.tools.npm.utils import (
    run_npm,
    SHRINKWRAP,
)

def npm_install(shrinkwrap_path, output):
    shutil.copyfile(shrinkwrap_path, os.path.join(output, SHRINKWRAP))

    # For npm 5+, npm won't install from a shrinkwrap file if there's
    # no package.json file. Create a dummy package.json file in the
    # output directory to work around that.

    # First, read the version and name from the shrinkwrap.
    with open(os.path.join(output, SHRINKWRAP)) as f:
        shrinkwrap = json.load(f)

    # Get the dependency name and version
    encoded_dep_name = shrinkwrap['name']
    dep_name = encoded_dep_name[len('npm-gen-'):]
    dep_version = shrinkwrap['version']

    # Write out the package.json file.
    with open(os.path.join(output, "package.json"), "w") as out:
        json.dump({
            'name': encoded_dep_name,
            'version': dep_version,
            'dependencies': {
                dep_name: dep_version,
            },
        }, out)

    # Now finish the install.

    tmpdir = mkdtemp(suffix='npm_install')
    env = {
        # Change the npm cache to a tmp directory so that this can run
        # sandboxed.
        'NPM_CONFIG_CACHE': os.path.join(tmpdir, '.npm'),
    }

    run_npm(['install'], env=env, cwd=output)

    shutil.rmtree(tmpdir)


def main():
    parser = argparse.ArgumentParser(
        description="Install npm modules",
    )
    parser.add_argument("shrinkwrap",
                        help="Path to npm-shrinkwrap.json",
                        type=str)
    parser.add_argument("output",
                        help="Directory that you want to install into.",
                        type=str)

    args = parser.parse_args()

    npm_install(args.shrinkwrap, args.output)


if __name__ == '__main__':
    main()
