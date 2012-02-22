var _ = require('underscore');

function QueryStats(opts) {
  this.records = [];
  this.options = opts;
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

  if (this.options.timeout) {
    while ((now - this.records[0].end) > this.options.timeout)
      this.records.shift(1);
  }
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

  return stats;
}

module.exports = QueryStats;
