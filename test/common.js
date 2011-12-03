var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path'),
    parseopt   = require('parseopt');

var REPO;
var extra_args;
var parser = new parseopt.OptionParser(
  {
    options: [
      {
        name: "--querylist",
        default: path.join(__dirname, 'testcases'),
        type: 'string',
        help: 'Load an alternate list of query terms'
      }
    ]
  });
var opts;


function parseopts(argv) {
  opts = parser.parse(argv);
  REPO = opts.arguments[0];
  extra_args = opts.arguments.slice(1);
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
  return fs.readFileSync(opts.options.querylist, 'utf8').split(/\n/);
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
