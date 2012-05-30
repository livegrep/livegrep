"use strict";

var sqlite3 = require('sqlite3').verbose(),
    carrier = require('carrier'),
    fs      = require('fs');
var db;

function go() {
  db.run(
'CREATE TABLE IF NOT EXISTS query_log (\n' +
'  query_time TIMESTAMP, \n' +
'  line VARCHAR(255), \n' +
'  file VARCHAR(255), \n' +
'  remote_ip VARCHAR(255), \n' +
'  search_time INT \n' +
'); \n', insert_data)
}

function insert_data() {
  var stream = fs.createReadStream(process.argv[2], {
                                     encoding: 'utf8'
                                   });
  var stmt = db.prepare('INSERT INTO query_log VALUES (?, ?, ?, ?, ?)');
  var re = /^\[(\d{4}-\d{2}-\d{2} \d\d:\d\d:\d\d\.\d\d\d)\] \[DEBUG\] appserver - \[([0-9.]+):\d+\] Search done: \((.*)\): (\d+)/;
  var ct = 0;
  function handle_one(line) {
    var m = re.exec(line);
    if (!m)
      return;
    var ts = m[1],
        ip = m[2],
        q  = m[3],
        ms = parseInt(m[4]);
    try {
      q = JSON.parse(q);
    } catch(e) {
      console.error("parse error: %s: %s", q, e);
      return;
    }
    stmt.run(ts, q.line, q.file, ip, ms);
    if (0 == (++ct % 1000))
      console.log("%d...", ct);
  }
  carrier.carry(stream, handle_one);
  stream.on('end', function() {
              stmt.finalize(done);
            });
}

function done() {
  console.log("Done.");
  db.close()
  process.exit(0);
}

db = new sqlite3.Database('logs.sqlite', go);
