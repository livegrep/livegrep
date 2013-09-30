function Batch(cb, interval, preprocess) {
  if (interval === undefined)
    interval = 10;
  this.cb         = cb;
  this.preprocess = preprocess;
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
  if (this.preprocess)
    this.results = this.preprocess(this.results);
  this.results.forEach(this.cb);
  this.results = [];
  this.last_flush = new Date();
}

module.exports = Batch;
