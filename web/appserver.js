var Codesearch = require('./codesearch.js');

var REPO = '/home/nelhage/code/codesearch/'

var searcher = new Codesearch(REPO);

function Server(remote, conn) {
  this.pending_search = null;
  this.last_search = null;
  this.new_search = function(str) {
    if (str === this.last_search)
      return;
    this.pending_search = str;
    if (searcher.readyState == 'ready') {
      this.dispatch_search();
    }
  }
  searcher.on('ready', function () {
                this.dispatch_search();
              }.bind(this));
}

Server.prototype.dispatch_search = function() {
  if (this.pending_search !== null) {
    this.last_search = this.pending_search;
    console.log('dispatching: %s...', this.pending_search)
    var search = searcher.search(this.pending_search);
    this.pending_search = null;
    search.on('error', function () {});
    search.on('match', function (match) {
      console.log("[%s]: %j", search.search, match);
    });
  }
}

module.exports = Server;
