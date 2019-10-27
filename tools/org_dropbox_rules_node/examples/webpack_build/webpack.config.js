var path = require('path');

var dbxBazelUtils = require('dbx-bazel-utils');

var env = dbxBazelUtils.initBazelEnv(__dirname);

module.exports = {
  entry: ['entry.ts'],
  output: {
    filename: 'bundle.js',
    path: env.outputRoot,
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        loader: 'ts-loader',
      },
    ],
  },

  resolve: {
    extensions: ['.ts', '.js'],
    modules: [
      path.resolve(__dirname, "src"),
      path.resolve(__dirname, "node_modules"),
    ],
  },
}
