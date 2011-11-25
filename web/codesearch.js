var spawn   = require('child_process').spawn,
    path    = require('path'),
    carrier = require('carrier'),
    util    = require('util'),
    events  = require("events"),
    log4js  = require('log4js');

function Codesearch(repo, refs, opts) {
  if (opts === null)
    opts = {};
  events.EventEmitter.call(this);
  this.child = spawn(path.join(__dirname, '..', 'codesearch'),
                     ['--git_dir', path.join(repo, ".git"), '--json'].concat(
                       opts.args||[]).concat(refs || ['HEAD']),
                     {
                       customFds: [-1, -1, 2]
                     });
  this.child.stdout.setEncoding('utf8');
  this.child.on('exit', function(code) {
                  this.emit('error', 'Child exited with code ' + code);
                });
  carrier.carry(this.child.stdout, this.got_line.bind(this));
  this.readyState = 'init';
  this.current_search = null;
  this.logger = log4js.getLogger('codesearch');
}

util.inherits(Codesearch, events.EventEmitter);

Codesearch.prototype.search = function(str) {
  var evt;
  console.assert(this.readyState == 'ready');
  this.child.stdin.write(str + "\n");
  this.setState('searching');

  evt = new events.EventEmitter();
  evt.search = str;
  this.current_search = evt;
  return evt;
}

Codesearch.prototype.got_line = function(line) {
  this.logger.trace("< %s", line);
  this.handle_line[this.readyState].call(this, line);
}

function expect_ready(line) {
  console.assert(line == 'READY');
  this.ready();
}

Codesearch.prototype.handle_line = {
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

Codesearch.prototype.ready = function() {
  this.setState('ready');
  this.emit('ready');
}

Codesearch.prototype.error = function(err) {
  this.current_search.emit('error', err);
  this.endSearch();
}

Codesearch.prototype.endSearch = function() {
  this.setState('search_done');
  this.current_search = null;
}

Codesearch.prototype.match = function(match) {
  var evt = JSON.parse(match);
  this.current_search.emit('match', evt);
}

Codesearch.prototype.setState = function(state) {
  this.readyState = state;
}

module.exports = Codesearch;
