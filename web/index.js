#!/usr/bin/env node
var path    = require('path'),
    fs      = require('fs'),
    config  = require('./config.js'),
    spawn   = require('child_process').spawn;

console.log("Generating index file: %s", config.BACKEND.index);

var tmp = config.BACKEND.index + ".tmp";

var repos = config.BACKEND.repos.map(function (repo) {
  return (repo.name ? repo.name + "@" : "") +
      repo.path + ":" + repo.refs.join(",");
});

var cs = spawn(path.join(__dirname, '..', 'codesearch'),
               ['--dump_index', tmp,
                '--order_root', config.BACKEND.sort.join(' '),
               ].concat(repos),
               {
                 customFds: [-1, 1, 2]
               });
cs.on('exit', function(code) {
        if (code !== 0)
          console.error("Index process exited with error %d", code);
        fs.renameSync(tmp, config.BACKEND.index);
        process.exit(0);
      });
cs.stdin.end();
