var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    assert     = require('assert'),
    path       = require('path'),
    util       = require('util'),
    temp       = require('temp'),
    child_process = require('child_process'),
    common     = require('./common.js');

common.parser.add('--rebuild', {
  defaulte: false,
  type: 'flag',
  help: 'Compare the results of --load_index with not building an index.'
});
var opts = common.parseopts();

var cs_index = common.get_codesearch(['--threads=1', '--timeout=0']);
var cs_noindex;
if (opts.rebuild)
  cs_noindex = common.get_codesearch(['--threads=1', '--timeout=0', '--load_index=']);
else
  cs_noindex = common.get_codesearch(['--threads=1', '--noindex', '--timeout=0']);
var queries = common.load_queries();
var conn_index, conn_noindex;

var failures = 0;

function loop(i) {
  if (i == queries.length) {
    if (failures)
      console.log("FAIL");
    else
      console.log("OK");
    process.exit(failures);
  }

  console.log("%s ...", queries[i]);

  var need_matches = 2;
  var ready = 3;
  var matches = {};

  function compare(cb) {
    try {
      assert.deepEqual(matches.index,
                       matches.noindex,
                       "Matches: `" + queries[i] + "'");
      process.nextTick(cb);
    } catch (e) {
      failures++;
      console.log(e.message);
      var dir = temp.mkdirSync('codesearch.test');
      var tmp_noindex = path.join(dir, 'unindexed');
      var tmp_index   = path.join(dir, 'indexed');

      fs.writeFileSync(tmp_noindex, e.expected.map(
                         function (m) {
                           return util.format("%s:%d %s\n", m.file, m.lno, m.line);
                         }
                       ).join(""));
      fs.writeFileSync(tmp_index, e.actual.map(
                         function (m) {
                           return util.format("%s:%d %s\n", m.file, m.lno, m.line);
                         }
                       ).join(""));
      var diff = child_process.spawn('diff', ['-u', 'unindexed', 'indexed'], {
                                       cwd: dir
                                     });
      diff.stdout.on('data', function(data) {
                       process.stdout.write(data);
                     });
      diff.on('exit', function (code) {
                fs.unlinkSync(tmp_noindex);
                fs.unlinkSync(tmp_index);
                fs.rmdirSync(dir);
                cb();
              });
    }
  }

  function got_matches(which, cb) {
    return function (ms) {
      matches[which] = ms;
      if (--need_matches == 0) {
        compare(cb);
      }
    }
  }

  function one_ready() {
    if (--ready == 0)
      loop(i + 1)
  }

  conn_index.once('ready', one_ready);
  conn_noindex.once('ready', one_ready);

  common.query_all(conn_index, queries[i], got_matches('index', one_ready));
  common.query_all(conn_noindex, queries[i], got_matches('noindex', one_ready));
}

var ready = 2;

function one_ready() {
  if (--ready == 0)
    loop(0)
}

conn_index = cs_index.connect()
conn_index.once('ready', one_ready);

conn_noindex = cs_noindex.connect()
conn_noindex.once('ready', one_ready);
