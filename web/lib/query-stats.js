var _ = require('underscore');

var DEFAULT_DISPLAY_INTERVAL = 100;


function QueryStats(name, opts) {
  this.name    = name;
  this.records = [];
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
  if (!this.options.timeout)
    return;
  var now = new Date();
  while (recs.length && (now - recs[0].end) > this.options.timeout)
    recs.shift(1);
}

QueryStats.prototype.done = function(id, start, server_time) {
  var now = new Date();
  var rec = {
    id: id,
    time: now - start,
    server_time: server_time,
    start: start,
    end: now,
  };
  this.records.push(rec);
  this.queries++;

  return (this.queries % this.options.interval) === 0;
}

QueryStats.prototype.get_percentile = function (field, out) {
  var qs = _(this.records).sortBy(
    function (r) {
      return r[field];
    });

  [50, 90, 95, 99].forEach(
    function (n) {
      if (qs.length)
        out[n] = qs[Math.floor(n/100 * qs.length)][field];
      else
        out[n] = 0;
    });
}

QueryStats.prototype.stats = function() {
  var stats = {percentile: {},
               srv_percentile: {}};

  this.prune_old(this.records);

  this.get_percentile('time', stats.percentile);
  this.get_percentile('server_time', stats.srv_percentile);

  stats.queries = this.queries;

  var start = this.start_time;
  if (this.options.timeout) {
    start = Math.max(this.start_time, new Date() - this.options.timeout);
  }

  stats.qps = 1000 * this.records.length / (new Date() - start);

  return stats;
}

module.exports = QueryStats;
