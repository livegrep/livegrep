#!/usr/bin/env node
var path    = require('path'),
    fs      = require('fs'),
    config  = require('../js/config.js'),
    backend = require('../js/backend.js'),
    parseopt= require('../js/lib/parseopt.js'),
    spawn   = require('child_process').spawn;

function generateIndex(backend, cb) {
  console.log("Generating index file: %s", backend.index);

  var tmp = backend.index + ".tmp";

  var repos = backend.repos.map(function (repo) {
    return (repo.name ? repo.name + "@" : "") +
      repo.path + ":" + repo.refs.join(",");
    });

  var cs = spawn(path.join(__dirname, '..', 'codesearch'),
                 ['--dump_index', tmp,
                  '--order_root', backend.sort.join(' '),
                 ].concat(repos),
                 {
                   customFds: [-1, 1, 2]
                 });
  cs.on('exit', function(code, signal) {
          if (code !== 0) {
            console.error("Index process exited with %s", code ? ("error " + code) : "signal " + signal);
          } else {
            fs.renameSync(tmp, backend.index);
          }
          cb();
        });
  cs.stdin.end();
}

var parser = new parseopt.OptionParser();
backend.addBackendOpt(config, parser);

var opts = parser.parse(process.argv);
generateIndex(backend.selectBackend(config, opts),
             function () {
               process.exit(0);
             });
