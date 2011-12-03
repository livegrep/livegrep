var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path');

var REPO;
var extra_args;

function parseopts(argv) {
   REPO = argv[2] || '/home/nelhage/code/linux-2.6';
   extra_args = argv.slice(3);
}
module.exports.parseopts = parseopts;

function get_codesearch(args) {
  if (args === undefined)
    args = [];
  return new Codesearch(
    REPO, [], {
      args: args.concat(extra_args)
    });
}
module.exports.get_codesearch = get_codesearch;

function load_queries() {
  return fs.readFileSync(path.join(__dirname, 'testcases'), 'utf8').split(/\n/);
}
module.exports.load_queries = load_queries;

function query_all(cs, q, cb) {
  var search = cs.search(q);
  var matches = [];
  search.on('match', function (m) {
              matches.push(m);
            })
  search.on('done', function () {
              cb(matches)
            });
}
module.exports.query_all = query_all;
