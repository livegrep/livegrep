var spawn   = require('child_process').spawn,
    path    = require('path'),
    carrier = require('carrier'),
    util    = require('util'),
    events = require("events");

function Codesearch(dir, refs) {
  events.EventEmitter.call(this);
  this.child = spawn(path.join(__dirname, '..', 'codesearch'),
                     (refs || ['HEAD']), {
                       cwd: dir,
                       customFds: [-1, -1, 2]
                     });
  this.child.stdout.setEncoding('utf8');
  carrier.carry(this.child.stdout, this.got_line.bind(this));
  this.readyState = 'init';
  this.current_search = null;
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
  console.log("< " + line);
  this.handle_line[this.readyState].call(this, line);
}

function expect_ready(line) {
  console.assert(line == 'READY');
  this.ready();
}

Codesearch.prototype.handle_line = {
  'init': expect_ready,
  'searching': function (line) {
    var match = /^FATAL (.*)/.exec(line);
    if (match) {
      this.error(match[1]);
    } else if (line == 'DONE') {
      this.current_search.emit('done');
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
  this.emit('readystatechange', state);
}

module.exports = Codesearch;
