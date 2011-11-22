#!/usr/bin/env node
var dnode   = require('dnode'),
    config  = require('./config.js');

dnode({
        error: function (str, err) {
          console.error("Error: %s", err);
          process.exit(1);
        },
        match: function (str, m) {
          console.log("%s:%d %s", m.file, m.lno, m.line);
        },
        search_done: function (str, time) {
          process.exit(0);
        }
      }).connect(
        'localhost', config.DNODE_PORT,
        function (remote) {
          remote.new_search(process.argv[2]);
        });
