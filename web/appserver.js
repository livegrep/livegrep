var dnode  = require('dnode'),
    config = require('./config.js');

function Client(parent, remote) {
  this.parent = parent;
  this.remote = remote;
  this.pending_search = null;
  this.last_search = null;
}

Client.prototype.new_search = function (str) {
  if (str === this.last_search)
    return;
  this.pending_search = str;
  this.dispatch_search();
}

Client.prototype.dispatch_search = function() {
  if (this.pending_search !== null &&
      this.parent.codesearch &&
      this.parent.ready) {
    var start = new Date();
    this.last_search = this.pending_search;
    console.log('dispatching: %s...', this.pending_search)
    var search = this.pending_search;
    this.pending_search = null;
    var self   = this;
    var remote = this.remote;
    var cbs = {
      not_ready: function() {
        if (self.pending_search === null)
          self.pending_search = search;
      },
      error: function (err) {
        if (remote.error)
          remote.error(search, err)
      },
      match: function (match) {
        if (remote.match)
          remote.match(search, match);
      },
      done: function () {
        if (remote.search_done)
          remote.search_done(search, (new Date()) - start);
      }
    }
    this.parent.codesearch.try_search(search, cbs);
    this.parent.ready = false;
  }
}

function SearchServer() {
  var parent = this;
  this.codesearch = null;
  this.clients = {};
  this.ready   = false;

  function ready() {
    parent.ready = true;
    Object.keys(parent.clients).forEach(
      function (id) {
        parent.clients[id].dispatch_search();
      })
  }

  dnode({
          ready: function() {
            ready();
          }
        }).connect(
          'localhost', config.DNODE_PORT,
          function (remote, conn) {
            parent.codesearch = remote;
            conn.on('ready', ready);
            conn.on('reconnect', ready);
          }, {
            reconnect: 200
          });

  this.Server = function (remote, conn) {
    parent.clients[conn.id] = new Client(parent, remote);
    this.new_search = function(str) {
      parent.clients[conn.id].new_search(str);
    }
    conn.on('end', function() {
              delete parent.clients[conn.id];
            });
  }
}

module.exports = SearchServer;
