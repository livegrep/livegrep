var Codesearch = require('./codesearch.js');

var REPO = '/home/nelhage/code/linux-2.6/';
var REF  = 'refs/tags/v3.0';

var searcher = new Codesearch(REPO, [REF]);

var clients = {};

function Client(remote) {
  this.remote = remote;
  this.pending_search = null;
  this.last_search = null;
}

Client.prototype.new_search = function (str) {
  if (str === this.last_search)
    return;
  this.pending_search = str;
  if (searcher.readyState == 'ready') {
    this.dispatch_search();
  }
}

Client.prototype.dispatch_search = function() {
  if (this.pending_search !== null) {
    var start = new Date();
    this.last_search = this.pending_search;
    console.log('dispatching: %s...', this.pending_search)
    var search = searcher.search(this.pending_search);
    var remote = this.remote;
    this.pending_search = null;
    search.on('error', function (err) {
                if (remote.error)
                  remote.error(search.search, err)
              }.bind(this));
    search.on('match', function (match) {
                if (remote.match)
                  remote.match(search.search, match);
              });
    search.on('done', function () {
                if (remote.search_done)
                  remote.search_done(search.search, (new Date()) - start);
              });
  }
}


function Server(remote, conn) {
  clients[conn.id] = new Client(remote);
  this.new_search = function(str) {
    clients[conn.id].new_search(str);
  }
  conn.on('end', function() {
            delete clients[conn.id];
          });
}

searcher.on('ready', function () {
              Object.keys(clients).forEach(
                function (id) {
                  clients[id].dispatch_search();
                });
            });


module.exports = Server;
