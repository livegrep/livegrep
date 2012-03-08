var path   = require('path'),
    fs     = require('fs'),
    log4js = require('log4js');

var config = {
  DNODE_PORT: 0xC5EA,
  SEARCH_REPO: path.join(__dirname, "../../linux"),
  SEARCH_REF:  "v3.0",
  SEARCH_ARGS: [],
  BACKEND_CONNECTIONS: 4,
  BACKENDS: [
    ["localhost", 0xC5EA]
  ],
  LOG4JS_CONFIG: path.join(__dirname, "log4js.json")
};

try {
  fs.statSync(path.join(__dirname, 'config.local.js'));
  var local = require('./config.local.js');
  Object.keys(local).forEach(
    function (k){
      config[k] = local[k]
    })
} catch (e) {
}

log4js.configure(config.LOG4JS_CONFIG);

module.exports = config;
