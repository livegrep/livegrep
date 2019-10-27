"use strict";

var path = require('path');

var bazelEnv;

/**
 * Initialize the environment for building JS.
 *
 * Tries to make the environment consistent between Bazel and
 * non-Bazel builds by setting the working directory to `configDir`,
 * which is what you usually expect with builds that are run with `npm
 * run ...`.
 *
 * @param {string} configDir - The directory that the build config is in. You should usually
 *   just pass in `__dirname`.
 * @returns {Object} the config that should be used for the build. Looks like this:
 *   ```
 *   {
 *     'outputRoot': '...', # Directory to put the output files, relative to the BUILD file.
 *   }
 *   ```
 */
function initBazelEnv(configDir) {
  if (bazelEnv) {
    return bazelEnv;
  }

  if (!process.env.RUNFILES) {
    bazelEnv = {
      outputRoot: configDir,
    };
    return bazelEnv;
  }

  // The current directory is the bazel execRoot. After we get it, we
  // need to set the current working directory to the config dir.
  var bazelExecRoot = process.cwd();

  process.chdir(configDir);

  bazelEnv = {
    outputRoot: path.join(bazelExecRoot, process.env.BAZEL_OUTPUT_DIR),
  };

  return bazelEnv;
}

function validateRepoPath(repoPath) {
  // repoPath must start with '//' or '@'
  if (repoPath.indexOf('//') !== 0 && repoPath.indexOf('@') !== 0) {
    throw new Error('absolute Bazel path required ' + repoPath);
  }
  // repoPath cannot contain ':'
  if (repoPath.indexOf(':') !== -1) {
    throw new Error('absolute Bazel target not allowed - use path ' + repoPath);
  }
  var split = repoPath.split('/');
  for (var i = 0; i < split.length; i++) {
    var s = split[i];
    if (s === '.' || s === '..') {
      throw new Error('absolute Bazel path only - no relative paths ' + repoPath);
    }
  }
}

/**
 * @returns {string} full path to a resource referenced by the Bazel target
 */
function runfilesDataPath(repoPath) {
  validateRepoPath(repoPath);

  var runfilesDir = process.env.RUNFILES;
  if (!runfilesDir) {
    throw new Error("RUNFILES environment variable not defined");
  }

  if (runfilesDir.indexOf('@') === 0) {
    // Cut off '@' from repoPath and join. join returns a
    // normalized path.
    return path.join(runfilesDir, '..', repoPath.slice(1));
  }
  // Cut off '//' from repoPath and join.
  return path.join(runfilesDir, repoPath.slice(2));
}


module.exports = {
  initBazelEnv: initBazelEnv,
  runfilesDataPath: runfilesDataPath,
};

