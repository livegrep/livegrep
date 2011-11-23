var Codesearch = require('./codesearch.js'),
    git_util   = require('./git_util.js');

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
  if (this.parent.codesearch &&
      this.parent.codesearch.readyState == 'ready') {
    this.dispatch_search();
  }
}

Client.prototype.dispatch_search = function() {
  if (this.pending_search !== null) {
    var start = new Date();
    this.last_search = this.pending_search;
    console.log('dispatching: %s...', this.pending_search)
    var search = this.parent.codesearch.search(this.pending_search);
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

function SearchServer(repo, ref, args) {
  var parent = this;
  this.searcher = null;
  this.clients = {};

  git_util.rev_parse(
    repo, ref,
    function (err, sha1) {
      if (err) throw err;
      console.log("Searching commit %s (%s)",
                  ref, sha1);
      parent.codesearch = new Codesearch(repo, [sha1], {
                                           args: args
                                         });
      parent.codesearch.on('ready', function () {
                             Object.keys(parent.clients).forEach(
                               function (id) {
                                 parent.clients[id].dispatch_search();
                               });
                           });
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
