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

var DISPLAY_INTERVAL = 100;
var start = null;
var count = 0;

var QueryThread = (
  function () {
    var id = 0;
    return function (cs, queries) {
      this.connection = cs.connect();
      this.queries    = _.shuffle(queries.concat());
      this.i          = 0;
      this.id         = ++id;
      this.start_time = null;
      this.stats      = new QueryStats({timeout: 60*1000});
    }
  })();

QueryThread.prototype.start = function() {
  if (this.connection.readyState === 'ready')
    process.nextTick(this.step.bind(this));
  this.connection.on('ready', this.step.bind(this));
}

QueryThread.prototype.step = function() {
  if (start === null)
    start = new Date();

  var q = this.queries[(++this.i) % this.queries.length];
  this.start_time = new Date();
  this.query = q;
  var search = this.connection.search(q, null);
  search.on('done', this.done.bind(this));
}

QueryThread.prototype.done = function(stats) {
  count++;
  this.stats.done(this.i, this.start_time);
  if (this.i % DISPLAY_INTERVAL == 0)
    this.show_stats();
}

QueryThread.prototype.show_stats = function () {
  var stats = this.stats.stats();
  console.log("%d: %s/%s/%s/%s", this.id,
              stats.percentile[50],
              stats.percentile[90],
              stats.percentile[95],
              stats.percentile[99]);
  console.log("qps: %s", 1000 * count/(new Date() - start))
}

var qs = [];
for (var i = 0; i < opts.clients; i++) {
  var q = new QueryThread(cs, queries);
  qs.push(q);
  q.start();
}
