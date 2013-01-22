var path   = require('path'),
    fs     = require('fs'),
    log4js = require('log4js');

var config = {
  WEB_PORT: 8910,
  BACKEND: {
    host: "localhost",
    port: 0xC5EA,
    connections: 4,
    index: path.join(__dirname, "../../linux/codesearch.idx"),
    name: "linux",
    pretty_name: "Linux v3.7",
    repos: [
      {
        path: path.join(__dirname, "../../linux"),
        name: "",
        refs: ["v3.7"],
        github: "torvalds/linux",
      }
    ],
    sort: 'include kernel mm fs arch'.split(/\s+/),
  },
  LOG4JS_CONFIG:   path.join(__dirname, "log4js.json"),
  SLOW_THRESHOLD:  300,
  MIN_SLOW_TIME:   2000,
  MAX_SLOW_TIME:   10000,
  QUERY_STREAK:    5,
  SMTP_CONFIG:     null,
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

if (config.BACKEND.repos.length > 1) {
  var seen = {};
  config.BACKEND.repos.forEach(function (repo) {
    console.assert(repo.name);
    console.assert(!seen.hasOwnProperty(repo.name));
    seen[repo.name] = true;
  });
}

log4js.configure(config.LOG4JS_CONFIG);

module.exports = config;
