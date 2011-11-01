#!/usr/bin/env node
var express = require('express'),
    dnode   = require('dnode'),
    path    = require('path'),
    Server  = require('./appserver.js');

var REPO = '/home/nelhage/code/linux-2.6/';
var REF  = 'v3.0';


var app = express.createServer();
app.use(express.static(path.join(__dirname, 'static')));
app.get('/', function (req, res) {
          res.redirect('/index.html');
        })

app.listen(8910);
console.log("http://localhost:8910");

var server = dnode(new Server(REPO, REF).Server);
server.listen(app, {
                io: {
                  transports: ["htmlfile", "xhr-polling", "jsonp-polling"]
                }
              });
