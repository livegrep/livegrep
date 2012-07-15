#!/usr/bin/env node
var path    = require('path'),
    fs      = require('fs'),
    config  = require('./config.js'),
    spawn   = require('child_process').spawn;

console.log("Generating index file:")
console.log("  %s", config.SEARCH_INDEX),
console.log("from %s, ref:%s...", config.SEARCH_REPO, config.SEARCH_REF);

var tmp = config.SEARCH_INDEX + ".tmp";

var cs = spawn(path.join(__dirname, '..', 'codesearch'),
               ['--git_dir', path.join(config.SEARCH_REPO, ".git"),
                '--dump_index', tmp,
                '--order_root', config.ORDER_DIRS.join(' '),
                config.SEARCH_REF],
               {
                 customFds: [-1, 1, 2]
               });
cs.on('exit', function(code) {
        if (code !== 0)
          console.error("Index process exited with error %d", code);
        fs.renameSync(tmp, config.SEARCH_INDEX);
        process.exit(0);
      });
cs.stdin.end();
