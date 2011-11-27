var dnode  = require('dnode'),
    fs     = require('fs'),
    log4js = require('log4js'),
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
      this.parent.remotes.length) {
    var codesearch = this.parent.remotes.pop();
    console.assert(codesearch.cs_ready);
    var start = new Date();
    this.last_search = this.pending_search;
    this.parent.logger.debug('dispatching: %s...', this.pending_search);

    var search = this.pending_search;
    this.pending_search = null;
    var self   = this;
    var remote = this.remote;
    var cbs = {
      not_ready: function() {
        self.parent.logger.info('Remote reports not ready for %s', search);
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
      done: function (stats) {
        var time = (new Date()) - start;
        if (remote.search_done)
          remote.search_done(search, time);
        self.parent.logger.info("Search done: %s: %s: %j",
                                search, time, stats);
      }
    }
    codesearch.try_search(search, cbs);
    codesearch.cs_ready = false;
  }
}

function SearchServer() {
  var parent = this;
  this.remotes = [];
  this.clients = {};
  this.logger  = log4js.getLogger('appserver');

  var remote = null;
  function ready() {
    parent.logger.debug('Remote ready!');
    if (remote.cs_ready) {
      parent.logger.debug('(already queued)!');
      return;
    }
    remote.cs_ready = true;
    parent.remotes.push(remote);
    parent.dispatch();
  }

  dnode({ ready: ready }).
    connect(
      'localhost', config.DNODE_PORT,
      function (r, conn) {
        r.cs_ready = false;
        remote = r;
        parent.logger.info("Connected to codesearch daemon.");
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

SearchServer.prototype.dispatch = function () {
  var clients = this.clients;
  Object.keys(this.clients).forEach(
    function (id) {
      clients[id].dispatch_search();
    })
}

module.exports = SearchServer;
