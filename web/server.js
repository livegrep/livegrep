#!/usr/bin/env node
var express = require('express'),
    dnode   = require('dnode'),
    path    = require('path'),
    AppServer  = require('./appserver.js');

var app = express.createServer();
app.use(express.static(path.join(__dirname, 'static')));
app.get('/', function (req, res) {
          res.redirect('/index.html');
        })

app.listen(8910);
console.log("http://localhost:8910");

var server = dnode(AppServer);
server.listen(app, {
                io: {
                  transports: ["htmlfile", "xhr-polling", "jsonp-polling"]
                }
              });
