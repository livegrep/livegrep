function Batch(cb, interval) {
  if (interval === undefined)
    interval = 10;
  this.cb         = cb;
  this.interval   = interval;
  this.results    = [];
  this.last_flush = new Date();
}

Batch.prototype.send = function(r) {
  this.results.push(r);
  this.maybe_flush();
}

Batch.prototype.maybe_flush = function(r) {
  if ((new Date() - this.last_flush) > this.interval)
    this.flush();
}

Batch.prototype.flush = function() {
  this.results.forEach(this.cb);
  this.results = [];
  this.last_flush = new Date();
}

module.exports = Batch;
