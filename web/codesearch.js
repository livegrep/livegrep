var spawn   = require('child_process').spawn,
    path    = require('path'),
    carrier = require('carrier'),
    util    = require('util'),
    events  = require("events"),
    fs      = require('fs'),
    net     = require('net'),
    temp    = require('temp'),
    log4js  = require('log4js');

var logger = log4js.getLogger('codesearch');

var uniq = 0;

function Codesearch(repo, refs, opts) {
  if (opts === null)
    opts = {};
  events.EventEmitter.call(this);
  var socket = path.join(temp.mkdirSync('codesearch'), 'socket');

  this.socket = socket;
  refs = refs || ['HEAD'];
  var spec = repo + ":" + refs.join(",");
  this.child = spawn(path.join(__dirname, '..', 'codesearch'),
                     ['--json', '--listen', socket].concat(
                       opts.args||[]).concat(spec),
                     {
                       customFds: [-1, 1, 2]
                     });
  this.child.on('exit', function(code) {
                  this.emit('error', 'Child exited with code ' + code);
                });
  var child = this.child;
  process.on('exit', function() {
               child.kill();
               fs.unlinkSync(socket);
               fs.rmdirSync(path.dirname(socket));
             });
}
util.inherits(Codesearch, events.EventEmitter);

Codesearch.prototype.connect = function(cb) {
  var conn = new Connection(this);
  if (cb !== undefined)
    conn.on('connected', cb.bind(null, conn));
  return conn;
}

function Connection(parent) {
  var self = this;
  self.parent = parent;
  self.id     = ++uniq;
  function connect() {
    if (!fs.existsSync(parent.socket)) {
      logger.debug("Waiting for daemon startup...");
      setTimeout(connect, 100);
      return;
    }
    self.socket = net.connect(
      parent.socket,
      function() {
        self.emit('connected');
        self.socket.setEncoding('utf8');
        carrier.carry(self.socket,
                      self.got_line.bind(self));
        self.readyState = 'init';
      });
  }
  connect();

  self.readyState = 'connecting';
  self.current_search = null;
}
util.inherits(Connection, events.EventEmitter);

Connection.prototype.search = function(str, file) {
  var evt;
  logger.debug("[cs %s] search(%j)", this.id, {line: str, file: file});
  console.assert(this.readyState == 'ready');
  this.socket.write(JSON.stringify({line: str, file: file}) + "\n");
  this.setState('searching');

  evt = new events.EventEmitter();
  evt.search = {line: str, file: file};
  this.current_search = evt;
  return evt;
}

Connection.prototype.got_line = function(line) {
  logger.trace("< %s", line);
  this.handle_line[this.readyState].call(this, line);
}

function expect_ready(line) {
  console.assert(line == 'READY');
  this.ready();
}

Connection.prototype.handle_line = {
  'init': expect_ready,
  'searching': function (line) {
    var match;
    if (match = /^FATAL (.*)/.exec(line)) {
      this.error(match[1]);
    } else if (match = /^DONE\s*(.*)/.exec(line)) {
      var stats = JSON.parse(match[1]);
      this.current_search.emit('done', stats);
      this.endSearch();
    } else {
      this.match(line);
    }
  },
  'search_done': expect_ready,
  'ready': function () {
    console.assert(false);
  }
}

Connection.prototype.ready = function() {
  logger.debug("[cs %s] ready", this.id);
  this.setState('ready');
  this.emit('ready');
}

Connection.prototype.error = function(err) {
  this.current_search.emit('error', err);
  this.endSearch();
}

Connection.prototype.endSearch = function() {
  logger.debug("[cs %s] search_done(%j)", this.id, this.current_search);
  this.setState('search_done');
  this.current_search = null;
}

Connection.prototype.match = function(match) {
  var evt = JSON.parse(match);
  this.current_search.emit('match', evt);
}

Connection.prototype.setState = function(state) {
  this.readyState = state;
}

module.exports = Codesearch;
