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

QueryStats.prototype.done = function(id, start, user) {
  var now = new Date();
  var rec = {
    id: id,
    time: now - start,
    start: start,
    end: now,
    user: user
  };
  this.records.push(rec);
  this.queries++;

  if (this.options.timeout) {
    while ((now - this.records[0].end) > this.options.timeout)
      this.records.shift(1);
  }

  return (this.queries % this.options.interval) === 0;
}

QueryStats.prototype.stats = function() {
  var qs = _(this.records).sortBy(
    function (r) {
      return r.time;
    });

  var stats = {percentile: {}};

  [50, 90, 95, 99].forEach(
    function (n) {
      stats.percentile[n] = qs[Math.floor(n/100 * qs.length)].time;
    });

  stats.qps = 1000 * this.queries / (new Date() - this.start_time);

  return stats;
}

module.exports = QueryStats;
