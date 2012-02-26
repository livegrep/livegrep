var Codesearch = require('../web/codesearch.js'),
    _          = require('underscore'),
    fs         = require('fs'),
    assert     = require('assert'),
    path       = require('path'),
    common     = require('./common.js'),
    QueryStats = require('../lib/query-stats.js');

common.parser.add('--clients', {
                    default: 8,
                    type: 'integer',
                  });
var opts = common.parseopts();

var cs = common.get_codesearch();
var queries = common.load_queries();

var count = 0;

var QueryThread = (
  function () {
    var id = 0;
    return function (cs, queries, stats) {
      this.connection = cs.connect();
      this.queries    = _.shuffle(queries.concat());
      this.i          = 0;
      this.id         = ++id;
      this.start_time = null;
      this.stats      = stats;
    }
  })();

QueryThread.prototype.start = function() {
  if (this.connection.readyState === 'ready')
    process.nextTick(this.step.bind(this));
  this.connection.on('ready', this.step.bind(this));
}

QueryThread.prototype.step = function() {
  var q = this.queries[(++this.i) % this.queries.length];
  this.start_time = new Date();
  this.query = q;
  this.stats.start(this.i);
  var search = this.connection.search(q, null);
  search.on('done', this.done.bind(this));
}

QueryThread.prototype.done = function(stats) {
  count++;
  if (this.stats.done(this.i, this.start_time))
    this.show_stats();
}

QueryThread.prototype.show_stats = function () {
  var stats = this.stats.stats();
  console.log("%s/%s/%s/%s",
              stats.percentile[50],
              stats.percentile[90],
              stats.percentile[95],
              stats.percentile[99]);
  console.log("qps: %s", stats.qps)
}

var stats = new QueryStats({timeout: 60*1000});
var qs = [];
for (var i = 0; i < opts.clients; i++) {
  var q = new QueryThread(cs, queries, stats);
  qs.push(q);
  q.start();
}
