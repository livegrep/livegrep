#!/usr/bin/env node
var express = require('express'),
    dnode   = require('dnode'),
    path    = require('path');

var app = express.createServer();
app.use(express.static(path.join(__dirname, 'static')));
app.get('/', function (req, res) {
          res.redirect('/index.html');
        })

app.listen(8910);
console.log("http://localhost:8910");

function Server(remote, conn) {
    this .new_search = function(str) {
      console.log("Search for: " + str);
    }
}

var server = dnode(Server);
server.listen(app, {
                io: {
                  transports: ["htmlfile", "xhr-polling", "jsonp-polling"]
                }
              });
