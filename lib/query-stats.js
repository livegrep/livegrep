var _ = require('underscore');

var DEFAULT_DISPLAY_INTERVAL = 100;


function QueryStats(name, opts) {
  this.name    = name;
  this.by_client = [];
  this.by_server = [];
  this.queries = 0;
  this.options = opts;
  this.start_time   = null;
  if (!('interval' in this.options))
    this.options.interval = DEFAULT_DISPLAY_INTERVAL;
}

QueryStats.prototype.start = function(id) {
  if (this.start_time === null)
    this.start_time = new Date();
}


QueryStats.prototype.prune_old = function(recs) {
  var now = new Date();
  while ((now - recs[0].end) > this.options.timeout)
    recs.shift(1);
}

QueryStats.prototype.done = function(id, start, server_time) {
  var now = new Date();
  var rec = {
    id: id,
    time: now - start,
    start: start,
    end: now,
  };
  this.by_client.push(rec);
  rec = {
    id: id,
    time: server_time,
    start: start,
    end: now,
  };
  this.by_server.push(rec);
  this.queries++;

  if (this.options.timeout) {
    this.prune_old(this.by_client);
    this.prune_old(this.by_server);
  }

  return (this.queries % this.options.interval) === 0;
}

QueryStats.prototype.get_percentile = function (records, out) {
  var qs = _(records).sortBy(
    function (r) {
      return r.time;
    });

  [50, 90, 95, 99].forEach(
    function (n) {
      out[n] = qs[Math.floor(n/100 * qs.length)].time;
    });
}

QueryStats.prototype.stats = function() {

  var stats = {percentile: {},
               srv_percentile: {}};

  this.get_percentile(this.by_client, stats.percentile);
  this.get_percentile(this.by_server, stats.srv_percentile);

  stats.qps = 1000 * this.queries / (new Date() - this.start_time);

  return stats;
}

module.exports = QueryStats;
