#!/usr/bin/env node
var dnode   = require('dnode'),
    path    = require('path'),
    config  = require('./config.js'),
    git_util   = require('./git_util.js'),
    Codesearch = require('./codesearch.js');

var REPO = process.argv[2] || '/home/nelhage/code/linux-2.6/';
var REF  = process.argv[3] || 'v3.0';
var args = process.argv.slice(4);

/*
 * Used to invoke callbacks on remote objects, where they may or may not provide
 * a method of the appropriate name, or may provide something that is not even a
 * function.
 *
 * An alternate approach would be to validate remote objects as soon as we get
 * them, but that seems more error-prone, especially during prototyping.
 */
function remote_call(obj, fn) {
  var args = Array.prototype.slice.call(arguments, 2);
  try {
    obj[fn].apply(obj, args);
  } catch (e) {
    console.log("fn: %s", e);
  }
}

function Client(parent, remote) {
  this.parent = parent;
  this.remote = remote;
}

Client.prototype.ready = function() {
  remote_call(this.remote, 'ready');
}

Client.prototype.search = function (re, cb) {
  if (this.parent.codesearch.readyState !== 'ready') {
    this.parent.queue.push({
                             client: this,
                             re: re,
                             cb: cb
                           });
    return;
  }
  var search = this.parent.codesearch.search(re);
  search.on('error', remote_call.bind(null, cb, 'error'));
  search.on('done',  remote_call.bind(null, cb, 'done'));
  search.on('match', remote_call.bind(null, cb, 'match'));
}

function Server(repo, ref, args) {
  var parent = this;
  this.codesearch = null
  this.clients = [];
  this.queue   = [];

  git_util.rev_parse(
    repo, ref,
    function (err, sha1) {
      if (err) throw err;
      console.log("Searching commit %s (%s)", ref, sha1);
      parent.codesearch = new Codesearch(repo, [sha1], {
                                           args: args
                                         });

      parent.codesearch.on('ready', function () {
                             var q;
                             if (parent.queue.length) {
                               q = parent.queue.shift();
                               q.client.search.call(q.client, q.re, q.cb);
                             } else {
                               Object.keys(parent.clients).forEach(
                                 function (id) {
                                   parent.clients[id].ready();
                                 });
                             }
                           });
    });

  this.Server = function (remote, conn) {
    parent.clients[conn.id] = new Client(parent, remote);
    conn.on('end', function() {
              var client = parent.clients[conn.id];
              parent.queue = parent.queue.filter(
                function (q) {
                  return q.client !== client
                });
              delete parent.clients[conn.id];
            });
    this.try_search = function(re, cb) {
      if (parent.codesearch.readyState !== 'ready') {
        remote_call(cb, 'not_ready');
        return;
      }
      parent.clients[conn.id].search(re, cb);
    }
    this.search = function(re, cb) {
      parent.clients[conn.id].search(re, cb);
    }
  }
}

var server = dnode(new Server(REPO, REF, args).Server);
server.listen(config.DNODE_PORT);
