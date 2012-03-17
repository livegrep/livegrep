var io         = require('socket.io-client'),
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
common.parser.add('--slow-clients', {
                    default: 1,
                    type: 'integer',
                    target: 'slow_clients'
                  });

common.parser.add('--remote', {
                    default: 'http://localhost:8910/',
                    type: 'string'
                  });
var opts = common.parseopts();

// var cs = common.get_codesearch();
var queries = common.load_queries();

var count = 0;

var QueryThread = (
  function () {
    var id = 0;
    return function (queries, stats) {
      var self = this;
      this.connection = io.connect(opts.remote, {
                                     'force new connection': true,
                                     'transports': ['xhr']
                                   });
      this.ready      = false;
      this.connection.on('connect', function() {
                           console.log("[%s] connected to remote",
                                      stats.name);
                           self.ready = true;
                         });
      this.connection.on('search_done', this.done.bind(this));
      this.queries    = _.shuffle(queries.concat());
      this.i          = 0;
      this.id         = ++id;
      this.start_time = null;
      this.stats      = stats;
    }
  })();

QueryThread.prototype.start = function() {
  if (this.ready)
    process.nextTick(this.step.bind(this));
  this.connection.on('connect', this.step.bind(this));
}

QueryThread.prototype.step = function() {
  var q = this.queries[(++this.i) % this.queries.length];
  this.start_time = new Date();
  this.query = q;
  this.stats.start(this.i);
  this.connection.emit('new_search', q, null, null);
}

QueryThread.prototype.done = function(id, time) {
  count++;
  if (this.stats.done(this.i, this.start_time, time))
    this.show_stats();
  this.step();
}

QueryThread.prototype.show_stats = function () {
  var stats = this.stats.stats();
  console.log("[%s] %s/%s/%s/%s",
              this.stats.name,
              stats.percentile[50],
              stats.percentile[90],
              stats.percentile[95],
              stats.percentile[99]);
  console.log("[%s/bk] %s/%s/%s/%s",
              this.stats.name,
              stats.srv_percentile[50],
              stats.srv_percentile[90],
              stats.srv_percentile[95],
              stats.srv_percentile[99]);
  console.log("[%s] qps: %s",
              this.stats.name, stats.qps)
}

var stats = new QueryStats('main', {timeout: 60*1000});
var qs = [], slow_qs = [];
var q;
for (var i = 0; i < opts.clients; i++) {
  q = new QueryThread(queries, stats);
  qs.push(q);
  q.start();
}

var stats_slow = new QueryStats('slow', {timeout: 60*1000, interval: 50});
var slow_queries = fs.readFileSync(path.join(__dirname, 'slow'),
                                   'utf8').split(/\n/);
for (var i = 0; i < opts.slow_clients; i++) {
  q = new QueryThread(slow_queries, stats_slow);
  slow_qs.push(q);
  q.start();
}
