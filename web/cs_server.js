#!/usr/bin/env node
var dnode   = require('dnode'),
    path    = require('path'),
    config  = require('./config.js'),
    git_util   = require('./git_util.js'),
    util       = require('./util.js'),
    Codesearch = require('./codesearch.js');

function Client(parent, remote) {
  this.parent = parent;
  this.remote = remote;
}

Client.prototype.ready = function() {
  util.remote_call(this.remote, 'ready');
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
  search.on('error', util.remote_call.bind(null, cb, 'error'));
  search.on('done',  util.remote_call.bind(null, cb, 'done'));
  search.on('match', util.remote_call.bind(null, cb, 'match'));
}

function Server(config) {
  var parent = this;
  this.codesearch = null
  this.clients = [];
  this.queue   = [];

  git_util.rev_parse(
    config.SEARCH_REPO, config.SEARCH_REF,
    function (err, sha1) {
      if (err) throw err;
      console.log("Searching commit %s (%s)", config.SEARCH_REF, sha1);
      parent.codesearch = new Codesearch(config.SEARCH_REPO, [sha1], {
                                           args: config.SEARCH_ARGS
                                         }).connect();

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

var server = dnode(new Server(config).Server);
server.listen(config.DNODE_PORT);
