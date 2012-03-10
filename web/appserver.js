var dnode  = require('dnode'),
    fs     = require('fs'),
    log4js = require('log4js'),
    util   = require('util'),
    _      = require('underscore'),
    config = require('./config.js'),
    Batch  = require('./batch.js');
var logger  = log4js.getLogger('appserver');

function Client(parent, sock) {
  this.parent = parent;
  this.socket = sock;
  this.pending_search = null;
  this.last_search = null;
  this.active_search = null;
  this.remote_address = sock.handshake.address;
}

Client.prototype.debug = function() {
  logger.debug("[%s:%d] %s",
               this.remote_address.address,
               this.remote_address.port,
               util.format.apply(null, arguments));
}

Client.prototype.new_search = function (line, file, id) {
  this.debug('new search: (%s) (%j)', id, {line:line, file:file});
  if (this.last_search &&
      line === this.last_search.line &&
      file === this.last_search.file)
    return;
  this.pending_search = {
    line: line,
    file: file,
    id: id
  };
  this.dispatch_search();
}

Client.prototype.search_done = function() {
  this.active_search = null;
  process.nextTick(this.dispatch_search.bind(this));
}

Client.prototype.dispatch_search = function() {
  if (this.pending_search !== null &&
      !this.active_search &&
      this.parent.remotes.length) {
    var codesearch = this.parent.remotes.pop();
    console.assert(codesearch.cs_ready);
    var start = new Date();
    this.last_search = this.pending_search;
    this.debug('dispatching: (%j)...', this.pending_search);

    var search = this.pending_search;
    this.pending_search = null;
    this.active_search  = search;
    var self   = this;
    var sock   = this.socket;
    var batch  = new Batch(function (m) {
                             sock.emit('match', search.id, m);
                           }, 50);
    var cbs = {
      not_ready: function() {
        logger.info('Remote reports not ready for %j', search);
        if (self.pending_search === null)
          self.pending_search = search;
        self.search_done();
        codesearch.cs_client = null;
      },
      error: function (err) {
        sock.emit('regex_error', search.id, err);
        self.search_done();
        codesearch.cs_client = null;
      },
      match: function (match) {
        match = JSON.parse(match);
        logger.trace("Reporting match %j for %j.", match, search);
        batch.send(match);
      },
      done: function (stats) {
        stats = JSON.parse(stats);
        var time = (new Date()) - start;
        batch.flush();
        sock.emit('search_done', search.id, time, stats.why);
        self.debug("Search done: (%j): %s", search, time);
        self.search_done();
        codesearch.cs_client = null;
      }
    }
    codesearch.try_search(search.line, search.file, cbs);
    codesearch.cs_ready = false;
    codesearch.cs_client = this;
  }
}

function SearchServer(config, io) {
  var parent = this;
  this.config  = config;
  this.remotes = [];
  this.connections = [];
  this.clients = {};

  config.BACKENDS.forEach(
    function (bk) {
      for (var i = 0; i < config.BACKEND_CONNECTIONS; i++) {
        (function() {
           var remote = null;
           var connection = null;

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

           function disconnected() {
             logger.info("Lost connection to backend")
             parent.remotes = parent.remotes.filter(function (r) {return r !== remote});
             parent.connections = parent.connections.filter(
               function (c) { return c !== connection});
             if (remote.cs_client)
               remote.cs_client.search_done();
           }

           dnode({ ready: ready }).
             connect(
               bk[0], bk[1],
               function (r, conn) {
                 parent.connections.push(conn);
                 remote = r;
                 connection = conn;
                 remote.cs_ready  = false;
                 remote.cs_client = null;
                 logger.info("Connected to codesearch daemon.");
                 conn.on('ready',     ready);
                 conn.on('reconnect', ready);
                 conn.on('close',     disconnected);
                 conn.on('end',       disconnected);
               }, {
                 reconnect: 200
               });
         })();
      }
    });

  var Server = function (sock) {
    logger.info("New client [%j]", sock.handshake.address);
    parent.clients[sock.id] = new Client(parent, sock);
    sock.on('new_search', function(line, file, id) {
              if (id == null)
                id = line;
              parent.clients[sock.id].new_search(line, file, id);
    });
    sock.on('disconnect', function() {
              delete parent.clients[sock.id];
            });
  };

  io.sockets.on('connection', function(sock) {
    new Server(sock);
  });
}

SearchServer.prototype.dispatch = function () {
  var clients = this.clients;
  _.shuffle(_.values(this.clients)).forEach(
    function (client) {
      client.dispatch_search();
    });
}

module.exports = SearchServer;
