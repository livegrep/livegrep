var dnode  = require('dnode'),
    fs     = require('fs'),
    net    = require('net'),
    log4js = require('log4js'),
    util   = require('util'),
    path   = require('path'),
    _      = require('underscore'),
    Batch  = require('./batch.js'),
    QueryStats  = require('./lib/query-stats.js');
var logger  = log4js.getLogger('appserver');

function remote_address(sock) {
 if (sock.handshake.address)
   return sock.handshake.address;
  return {};
}

function Client(parent, sock) {
  this.parent = parent;
  this.is_fast = true;
  this.socket = sock;
  this.pending_search = null;
  this.last_search = null;
  this.active_search = null;
  this.remote_address = remote_address(sock);
  this.fast_streak = 0;
  this.last_slow   = null;
}

var DEFAULT_BACKEND = 'livegrep';

Client.prototype.pool = function(search) {
  return this.parent.pools[search.backend][this.is_fast ? "fast" : "slow"];
}

Client.prototype.debug = function() {
  logger.debug("[%s:%d] %s",
               this.remote_address.address,
               this.remote_address.port,
               util.format.apply(null, arguments));
}

Client.prototype.new_search = function (opts) {
  opts.backend = opts.backend || DEFAULT_BACKEND;
  this.debug('new search: %j', opts);
  if (this.last_search &&
      opts.line === this.last_search.line &&
      opts.file === this.last_search.file &&
      opts.backend === this.last_search.backend)
    return;
  if (opts.line === '') {
    this.last_search = null;
    return;
  }
  this.pending_search = opts;
  this.dispatch_search();
}

Client.prototype.search_done = function() {
  this.active_search = null;
  process.nextTick(this.dispatch_search.bind(this));
}

Client.prototype.mark_fast = function(fast) {
  if (this.is_fast === fast)
    return;
  this.debug("Switching to %s pools", fast ? "fast" : "slow");
  this.is_fast = fast;
}

Client.prototype.slow_query = function() {
  this.last_slow = new Date();
  this.fast_streak = 0;
  this.mark_fast(false);
}

Client.prototype.fast_query = function() {
  this.fast_streak++;
  if ((new Date() - this.last_slow) < this.parent.config.MIN_SLOW_TIME)
    return;
  if (this.fast_streak >= this.parent.config.QUERY_STREAK)
    this.mark_fast(true);
}

Client.prototype.sort_matches = function(pool, matches) {
  var order =  {};
  for (var i = 0; i < pool.backend.sort.length; i++)
    order[pool.backend.sort[i]] = i;
  function sort_order(path) {
    var dir = /^[^\/]+/.exec(path)[0];
    if (dir in order)
      return order[dir];
    return 1000;
  }
  i = 0;
  var annotated = matches.map(function (m) {
                                return {
                                  match: m,
                                  order: sort_order(m.file),
                                  index: i
                                }
                              });
  var sorted = annotated.sort(function (a,b) {
                                if (a.order != b.order)
                                  return a.order - b.order;
                                return a.index - b.index;
                              });
  return sorted.map(function (r) {return r.match});
}

Client.prototype.dispatch_search = function() {
  if (this.pending_search === null || this.active_search)
    return;
  var pool = this.pool(this.pending_search);
  if (!pool.remotes)
    return;

  if (this.last_slow &&
      (new Date() - this.last_slow) >= this.parent.config.MAX_SLOW_TIME)
    this.mark_fast(true);

  var codesearch = pool.remotes.pop();
  console.assert(codesearch.cs_ready);
  var start = new Date();
  this.last_search = this.pending_search;
  this.debug('dispatching: (%j) to %s-%d...',
    this.pending_search,
    pool.stats.name,
    codesearch.__id);

  var search = this.pending_search;
  this.pending_search = null;
  this.active_search  = search;
  var self   = this;
  var sock   = this.socket;
  var batch  = new Batch(function (m) {
                           sock.emit('match', search.id, m);
                         }, 50,
                         function (matches) {
                           return self.sort_matches(pool, matches)
                         });
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
      match.backend = search.backend;
      logger.trace("Reporting match %j for %j.", match, search);
      batch.send(match);
    },
    done: function (stats) {
      stats = JSON.parse(stats);
      var time = (new Date()) - start;
      pool.stats.done(search.id, start, time);
      batch.flush();
      sock.emit('search_done', search.id, time, stats.why);
      self.debug("Search done: (%j): %s", search, time);
      if (time > self.parent.config.SLOW_THRESHOLD) {
        self.slow_query();
      } else {
        self.fast_query();
      }
      self.search_done();
      codesearch.cs_client = null;
    }
  }
  codesearch.try_search(search.line, search.file, cbs);
  codesearch.cs_ready = false;
  codesearch.cs_client = this;
}

function ConnectionPool(server, name, backend) {
  var self = this;
  this.server  = server
  this.remotes = [];
  this.connections = [];
  this.stats       = new QueryStats(name, {timeout: 5*60*1000});
  this.stats.start();
  this.backend = backend;

  var id = 0;
  for (var i = 0; i < backend.connections; i++) {
    self.connect_to(backend, id++);
  }
}

ConnectionPool.prototype.connect_to = function(bk, id) {
  var self = this;
  var remote = null;
  var delay = 100;
  var d = null;
  var stream = null;
  var me = id;

  var ready = function() {
    if (!remote) return;
    logger.debug('Remote %s-%d ready (%d)!',
      self.stats.name, remote.__id, me);
    if (remote.cs_ready) {
      logger.debug('(already queued)!');
      return;
    }
    remote.cs_ready = true;
    self.remotes.push(remote);
    self.dispatch();
    delay = 100;
  }

  var disconnected = function() {
    logger.info("Lost connection to backend (%s-%s). Reconnect in %d...",
      self.stats.name, remote ? remote.__id : "??", delay);
    stream.end();
    self.remotes = self.remotes.filter(function (r) {return r !== remote});
    self.connections = self.connections.filter(
      function (c) { return c !== d});
    if (remote && remote.cs_client)
      remote.cs_client.search_done();
    setTimeout(connect, delay)
    delay = Math.min(delay * 2, 60*1000);
  }

  var connect = function() {
    d = dnode({ ready: ready });
    d.on('remote', function(r) {
           self.connections.push(d);
           remote = r;
           remote.__id = id;
           remote.cs_ready  = false;
           remote.cs_client = null;
           logger.info("Connected to codesearch daemon.");
         });
    d.on('ready',     ready);
    d.on('close',     disconnected);
    d.on('end',       disconnected);
    d.on('error',     disconnected);
    stream = net.connect({
                           host: bk.host,
                           port: bk.port
                         });
    stream.on('error',   disconnected);
    stream.pipe(d).pipe(stream);
  }

  connect();
}

ConnectionPool.prototype.dispatch = function () {
  var clients = this.clients;
  _.shuffle(_.values(this.clients)).forEach(
    function (client) {
      client.dispatch_search();
    });
}

function SearchServer(config, io) {
  var self = this;
  this.config  = config;
  this.clients = {};
  this.pools   = {};
  Object.keys(config.BACKENDS).forEach(function (name) {
    var backend = config.BACKENDS[name];
    self.pools[name] = {
      fast: new ConnectionPool(self, 'fast-' + name, backend),
      slow: new ConnectionPool(self, 'slow-' + name, backend),
    }
  });

  var Server = function (sock) {
    logger.info("New client (%s)[%j]", sock.id, remote_address(sock));
    self.clients[sock.id] = new Client(self, sock);
    sock.on('new_search', function(opts, file, id) {
              if (typeof(opts) == 'string') {
                // Compatibility with the old API
                opts = {
                  line: opts,
                  file: file,
                  id: id
                }
              }
              if (opts.id == null)
                opts.id = opts.line;
              self.clients[sock.id].new_search(opts);
    });
    sock.on('disconnect', function() {
              logger.info("Disconnected (%s)[%j]", sock.id, remote_address(sock));
              delete self.clients[sock.id];
            });
  };

  io.sockets.on('connection', function(sock) {
    new Server(sock);
  });
  setInterval(function() {
                self.dump_stats();
              }, 30*1000);
}

SearchServer.prototype.dump_stats = function() {
  var clients = 0, slow = 0, fast = 0;
  var self = this;
  _.values(this.clients).forEach(
    function (c){
      clients++;
      if (c.is_fast)
        fast++;
      else
        slow++;
    });
  logger.info("Clients/slow/fast: %d %d %d", clients, slow, fast);
  var stats = {};
  Object.keys(self.pools).forEach(function (name) {
    var pools = self.pools[name];
    stats[pools.slow.stats.name] = pools.slow.stats.stats();
    stats[pools.fast.stats.name] = pools.slow.stats.stats();
  });
  stats.server = {
    clients: clients,
    slow: slow,
    fast: fast
  };
  fs.writeFile(path.join(__dirname, "log/stats.json"),
               JSON.stringify(stats) + "\n",
               "utf8");
}

module.exports = SearchServer;
