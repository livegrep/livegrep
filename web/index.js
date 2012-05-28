#!/usr/bin/env node
var path    = require('path'),
    config  = require('./config.js'),
    spawn   = require('child_process').spawn;

var cs = spawn(path.join(__dirname, '..', 'codesearch'),
               ['--git_dir', path.join(config.SEARCH_REPO, ".git"),
                '--dump_index', config.SEARCH_INDEX,
                '--order_root', config.ORDER_DIRS.join(' '),
                config.SEARCH_REF],
               {
                 customFds: [-1, 1, 2]
               });
cs.on('exit', function(code) {
        if (code !== 0)
          console.error("Index process exited with error %d", code);
        process.exit(0);
      });
cs.stdin.end();
