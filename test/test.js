var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    assert     = require('assert'),
    path       = require('path');

var REPO = process.argv[2] || '/home/nelhage/code/linux-2.6';

var extra_args = process.argv.slice(3);

var cs_index = new Codesearch(REPO, [], {
                                args: ['--threads=1', '--timeout=0'].concat(extra_args)
                              });

var cs_noindex = new Codesearch(REPO, [], {
                                  args: ['--threads=1', '--noindex', '--timeout=0'].
                                    concat(extra_args)
                                });

var queries = fs.readFileSync(path.join(__dirname, 'testcases'), 'utf8').split(/\n/);

function queryAll(cs, q, cb) {
  var search = cs.search(q);
  var matches = [];
  search.on('match', function (m) {
              matches.push(m);
            })
  search.on('done', function () {
              cb(matches)
            });
}

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
  var ready = 2;
  var matches = {};

  function compare() {
    try {
      assert.deepEqual(matches.index,
                       matches.noindex,
                       "Matches: `" + queries[i] + "'");
    } catch (e) {
      console.log(e.message);
      console.log("Non-Indexed:");
      e.expected.forEach(
        function (m) {
          console.log("  - %s:%d %s", m.file, m.lno, m.line);
        });
      console.log("Indexed:");
      e.actual.forEach(
        function (m) {
          console.log("  - %s:%d %s", m.file, m.lno, m.line);
        });
      failures++;
    }
  }

  function got_matches(which) {
    return function (ms) {
      matches[which] = ms;
      if (--need_matches == 0) {
        compare();
      }
    }
  }

  function one_ready() {
    if (--ready == 0)
      loop(i + 1)
  }

  cs_index.once('ready', one_ready);
  cs_noindex.once('ready', one_ready);

  queryAll(cs_index, queries[i], got_matches('index'));
  queryAll(cs_noindex, queries[i], got_matches('noindex'));
}

var ready = 2;

function one_ready() {
  if (--ready == 0)
    loop(0)
}

cs_noindex.once('ready', one_ready);
cs_index.once('ready', one_ready);
