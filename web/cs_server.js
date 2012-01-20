#!/usr/bin/env node
var dnode   = require('dnode'),
    path    = require('path'),
    config  = require('./config.js'),
    git_util   = require('./git_util.js'),
    util       = require('./util.js'),
    Codesearch = require('./codesearch.js'),
    Batch      = require('./batch.js');

function Client(parent, remote) {
  var self = this;
  this.parent = parent;
  this.remote = remote;
  this.queue  = [];
  this.conn   = parent.codesearch.connect();
  this.conn.on('ready', function() {
                 var q;
                 if (self.queue.length) {
                   q = self.queue.shift();
                   self.search(q.re, q.cb);
                 } else {
                   self.ready();
                 }
               });
}

Client.prototype.ready = function() {
  util.remote_call(this.remote, 'ready');
}

Client.prototype.search = function (re, cb) {
  if (this.conn.readyState !== 'ready') {
    this.queue.push({
                      re: re,
                      cb: cb
                    });
    return;
  }
  var search = this.conn.search(re);
  var batch  = new Batch(function (m) {
                           util.remote_call(cb, 'match', m);
                         });
  search.on('error', util.remote_call.bind(null, cb, 'error'));
  search.on('done',  function () {
              batch.flush();
              util.remote_call.apply(null, [cb, 'done'].concat(Array.prototype.slice.call(arguments)));
            });
  search.on('match', batch.send.bind(batch));
}

function Server(config) {
  var parent = this;
  this.clients = [];

  this.codesearch = new Codesearch(config.SEARCH_REPO, [config.sha1], {
                                     args: config.SEARCH_ARGS
                                   });
  this.Server = function (remote, conn) {
    parent.clients[conn.id] = new Client(parent, remote);
    conn.on('end', function() {
              var client = parent.clients[conn.id];
              delete parent.clients[conn.id];
            });
    this.try_search = function(re, cb) {
      if (parent.clients[conn.id].conn.readyState !== 'ready') {
        util.remote_call(cb, 'not_ready');
        return;
      }
      parent.clients[conn.id].search(re, cb);
    }
    this.search = function(re, cb) {
      parent.clients[conn.id].search(re, cb);
    }
  }
}

var server = null;

git_util.rev_parse(
  config.SEARCH_REPO, config.SEARCH_REF,
  function (err, sha1) {
    if (err) throw err;
    console.log("Searching commit %s (%s)", config.SEARCH_REF, sha1);
    config.sha1 = sha1;
    server = dnode(new Server(config).Server);
    server.listen(config.DNODE_PORT);
  });
