var spawn   = require('child_process').spawn,
    path    = require('path'),
    carrier = require('carrier');

function Codesearch(dir, refs) {
  this.child = spawn(path.join(__dirname, '..', 'codesearch'),
                     (refs || ['HEAD']), {
                       cwd: dir,
                       customFds: [-1, -1, 2]
                     });
  this.child.stdout.setEncoding('utf8');
  carrier.carry(this.child.stdout, this.got_line.bind(this));
  this.readyState = 'init';
}

Codesearch.prototype.search = function(str) {
  console.assert(this.readyState == 'ready');
  this.child.stdin.write(str + "\n");
  this.readyState = 'searching';
}

Codesearch.prototype.got_line = function(line) {
  console.log("< "+ line);
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
      this.readyState = 'search_done';
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
  this.readyState = 'ready';
}

Codesearch.prototype.error = function(err) {
  console.log("ERROR: " + err);
  this.readyState = 'search_done';
}

Codesearch.prototype.match = function(match) {
  console.log("MATCH: " + match);
}

module.exports = Codesearch;
