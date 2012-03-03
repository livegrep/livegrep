var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path'),
    parseopt   = require('parseopt'),
    log4js     = require('log4js'),
    config     = require('../web/config.js');

log4js.configure({
                   levels: {
                     'codesearch': 'FATAL'
                   }
                 });

var extra_args;
var parser = new parseopt.OptionParser(
  {
    options: [
      {
        name: "--querylist",
        default: path.join(__dirname, 'testcases'),
        type: 'string',
        help: 'Load an alternate list of query terms'
      },
      {
        name: "--repo",
        default: config.SEARCH_REPO,
        type: 'string',
        help: 'Git repository to search'
      },
      {
        name: "--ref",
        default: config.SEARCH_REF,
        type: 'string',
        help: 'Git ref to search.'
      },
      {
        name: "--noempty",
        default: false,
        type: 'flag',
        help: 'Do not search for the empty string'
      }
    ]
  });
var opts;

module.exports.parser = parser;

function parseopts(argv) {
  opts = parser.parse(argv);
  extra_args = opts.arguments.slice();
  return opts.options;
}
module.exports.parseopts = parseopts;

function get_codesearch(args) {
  if (args === undefined)
    args = [];
  return new Codesearch(
    opts.options.repo, [opts.options.ref], {
      args: config.SEARCH_ARGS.concat(args).concat(extra_args)
    });
}
module.exports.get_codesearch = get_codesearch;

function load_queries() {
  var qs = fs.readFileSync(opts.options.querylist, 'utf8').split(/\n/);
  if (opts.options.noempty) {
    qs = qs.filter(function (s) {return s.length > 0;});
  }
  return qs;
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
