import os
import sys

class RunfilesError(Exception):
    pass

_runfiles_dir = None
def _get_runfiles_dir():
    if _runfiles_dir:
        return _runfiles_dir

    path_parts = os.path.abspath(sys.argv[0]).split('/')
    for i, part in enumerate(path_parts):
        if part.endswith('.runfiles'):
            # Include workspace name in runfiles path.
            return os.path.realpath('/'.join(path_parts[:i+2]))

    raise RunfilesError('Must be run in Bazel environment')

def _validate_repo_path(repo_path):
    # type: (str) -> None
    if not repo_path.startswith(('//', '@')):
        raise RunfilesError('absolute Bazel path required', repo_path)
    if ':' in repo_path:
        raise RunfilesError('absolute Bazel target not allowed - use path',
                            repo_path)
    for x in repo_path.split('/'):
        if x in ('.', '..'):
            raise RunfilesError('absolute Bazel path only - no relative paths',
                                repo_path)

# Return a full path to a resource referenced by the Bazel target path.
def data_path(repo_path):
    # type: (str) -> str
    _validate_repo_path(repo_path)

    runfiles_dir = _get_runfiles_dir()

    if repo_path.startswith('@'):
        return os.path.normpath(os.path.join(runfiles_dir, '..', repo_path[1:]))
    return os.path.join(runfiles_dir, repo_path[2:])
