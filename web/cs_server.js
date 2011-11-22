#!/usr/bin/env node
var dnode   = require('dnode'),
    path    = require('path'),
    Server  = require('./appserver.js'),
    config  = require('./config.js');

var REPO = process.argv[2] || '/home/nelhage/code/linux-2.6/';
var REF  = process.argv[3] || 'v3.0';
var args = process.argv.slice(4);

var server = dnode(new Server(REPO, REF, args).Server);
server.listen(config.DNODE_PORT);
