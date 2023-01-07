var path = require('path');
var env = { outputRoot: path.resolve(__dirname) };

var webpack = require('webpack');

module.exports = {
  entry: 'entry.js',
  output: {
    filename: 'htdocs/assets/js/bundle.js',
    path: env.outputRoot,
  },

  //devtool: 'eval-cheap-module-source-map',

  externals: {
    jquery: 'jQuery',
    underscore: '_',
    backbone: 'Backbone',
    'js-cookie': 'Cookies'
  },

  module: {
    loaders: [
      { test: /\.css$/, loaders: ['style-loader', 'css-loader'] },
    ]
  },

  resolve: {
    modules: [
      path.resolve(__dirname, "src"),
      path.resolve(__dirname, "node_modules")
    ],
    alias: {
      'html': path.resolve(__dirname, "3d/html")
    }
  },

  plugins: [
    new webpack.optimize.UglifyJsPlugin()
  ]
}
