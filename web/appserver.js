var dnode  = require('dnode'),
    fs     = require('fs'),
    log4js = require('log4js'),
    util   = require('./util.js'),
    config = require('./config.js'),
    Batch  = require('./batch.js');
var logger  = log4js.getLogger('appserver');

function Client(parent, sock) {
  this.parent = parent;
  this.socket = sock;
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
    logger.debug('dispatching: %s...', this.pending_search);

    var search = this.pending_search;
    this.pending_search = null;
    var self   = this;
    var sock   = this.socket;
    var batch  = new Batch(function (m) {
                             sock.emit('match', search, m);
                           });
    var cbs = {
      not_ready: function() {
        logger.info('Remote reports not ready for %s', search);
        if (self.pending_search === null)
          self.pending_search = search;
      },
      error: function (err) {
        sock.emit('regex_error', search, err);
      },
      match: function (match) {
        match = JSON.parse(match);
        logger.trace("Reporting match %j for %s.", match, search);
        batch.send(match);
      },
      done: function (stats) {
        stats = JSON.parse(stats);
        var time = (new Date()) - start;
        batch.flush();
        sock.emit('search_done', search, time, stats.why);
        logger.debug("Search done: %s: %s", search, time);
      }
    }
    codesearch.try_search(search, cbs);
    codesearch.cs_ready = false;
  }
}

function SearchServer(config, io) {
  var parent = this;
  this.config  = config;
  this.remotes = [];
  this.connections = [];
  this.clients = {};

  for (var i = 0; i < config.BACKEND_CONNECTIONS; i++) {
    (function() {
       var remote = null;

       function ready() {
         logger.debug('Remote ready!');
         if (remote.cs_ready) {
           logger.debug('(already queued)!');
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
             parent.connections.push(conn);
             remote = r;
             remote.cs_ready = false;
             logger.info("Connected to codesearch daemon.");
             conn.on('ready', ready);
             conn.on('reconnect', ready);
           }, {
             reconnect: 200
           });
     })();
  }

  var Server = function (sock) {
    parent.clients[sock.id] = new Client(parent, sock);
    sock.on('new_search', function(str) {
      parent.clients[sock.id].new_search(str);
    });
    sock.on('disconnect', function() {
              delete parent.clients[sock.id];
            });
  };

  io.sockets.on('connection', function(sock) {
    new Server(sock);
  });
}

function shuffle(lst) {
  for (var i = lst.length - 1; i >= 0; i--) {
    var j = Math.floor(Math.random() * (i+1));
    var tmp = lst[i];
    lst[i] = lst[j];
    lst[j] = tmp;
  }
  return lst;
}

SearchServer.prototype.dispatch = function () {
  var clients = this.clients;
  shuffle(Object.keys(this.clients)).forEach(
    function (id) {
      clients[id].dispatch_search();
    })
}

module.exports = SearchServer;
