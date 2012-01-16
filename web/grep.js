#!/usr/bin/env node
var dnode   = require('dnode'),
    config  = require('./config.js');

var query = process.argv[2];
var dispatched = null;

var delegate = {
  error: function (err) {
    console.error("Error: %s", err);
    process.exit(1);
  },
  match: function (m) {
    m = JSON.parse(m);
    console.log("%s:%d %s", m.file, m.lno, m.line);
  },
  done: function () {
    process.exit(0);
  }
}

dnode().connect(
        'localhost', config.DNODE_PORT,
        function (remote) {
          remote.search(process.argv[2], delegate);
        });
