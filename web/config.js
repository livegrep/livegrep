var path   = require('path'),
    fs     = require('fs'),
    log4js = require('log4js');

var config = {
  DNODE_PORT: 0xC5EA,
  WEB_PORT: 8910,
  SEARCH_REPO_NAME: "Linux",
  SEARCH_REPO: path.join(__dirname, "../../linux"),
  SEARCH_REF:  "v3.6",
  SEARCH_INDEX: path.join(__dirname, "../../linux/codesearch.idx"),
  GITHUB_REPO: "torvalds/linux",
  SEARCH_ARGS: [],
  BACKEND_CONNECTIONS: 4,
  BACKENDS: [
    ["localhost", 0xC5EA]
  ],
  LOG4JS_CONFIG: path.join(__dirname, "log4js.json"),
  SLOW_THRESHOLD:  300,
  MIN_SLOW_TIME:   2000,
  MAX_SLOW_TIME:   10000,
  QUERY_STREAK:    5,
  SMTP_CONFIG:     null,
  ORDER_DIRS:      'include kernel mm fs arch'.split(/\s+/),
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
