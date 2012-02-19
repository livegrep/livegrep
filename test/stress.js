var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    assert     = require('assert'),
    path       = require('path'),
    common     = require('./common.js');

common.parseopts();

var cs = common.get_codesearch();
var queries = common.load_queries();

var QueryThread = (
  function () {
    var id = 0;
    return function (cs) {
      this.connection = cs.connect();
      this.i          = 0;
      this.id         = ++id;
      this.start_time = null;
    }
  })();

QueryThread.prototype.start = function() {
  if (this.connection.readyState === 'ready')
    process.nextTick(this.step.bind(this));
  this.connection.on('ready', this.step.bind(this));
}

QueryThread.prototype.step = function() {
  var q = queries[(++this.i) % queries.length];
  this.start_time = new Date();
  this.query = q;
  var search = this.connection.search(q, null);
  search.on('done', this.done.bind(this));
}

QueryThread.prototype.done = function() {
  var end = new Date();
  console.log("%d: %s", this.id, +(end - this.start_time));
}

var qs = [];
for (var i = 0; i < 8; i++) {
  var q = new QueryThread(cs);
  qs.push(q);
  q.start();
}
