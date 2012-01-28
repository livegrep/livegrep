#!/usr/bin/env node
var express = require('express'),
    dnode   = require('dnode'),
    path    = require('path'),
    parseopt= require('parseopt'),
    log4js = require('log4js'),
    Server  = require('./appserver.js'),
    config  = require('./config.js');

var parser = new parseopt.OptionParser(
  {
    options: [
      {
        name: "--autolaunch",
        default: false,
        type: 'flag',
        help: 'Automatically launch a code-search backend server.'
      }
    ]
  });

var opts = parser.parse();
if (!opts) {
  process.exit(1);
}

if (opts.options.autolaunch) {
  console.log("Autolaunching a back-end server...");
  require('./cs_server.js')
}

var app = express.createServer();
var logger = log4js.getLogger('web');
app.use(log4js.connectLogger(logger, {
                               level: log4js.levels.INFO,
                               format: ':remote-addr [:date] :method :url'
                             }));

app.use(express.static(path.join(__dirname, 'static')));
app.get('/', function (req, res) {
          res.redirect('/index.html');
        })

app.listen(8910);
console.log("http://localhost:8910");

var io = require('socket.io').listen(app, {
                                       'log level': 0
                                     });
io.configure(
  function() {
    io.enable('browser client gzip');
    io.set("transports", ["xhr-polling"]);
    io.set("polling duration", 10);

    var path = require('path');
    var HTTPPolling = require(path.join(
                                path.dirname(require.resolve('socket.io')),'lib', 'transports','http-polling')
                             );
    var XHRPolling = require(path.join(
                               path.dirname(require.resolve('socket.io')),'lib','transports','xhr-polling')
                            );

    XHRPolling.prototype.doWrite = function(data) {
      HTTPPolling.prototype.doWrite.call(this);

      var headers = {
        'Content-Type': 'text/plain; charset=UTF-8',
        'Content-Length': (data && Buffer.byteLength(data)) || 0
      };

      if (this.req.headers.origin) {
        headers['Access-Control-Allow-Origin'] = '*';
        if (this.req.headers.cookie) {
          headers['Access-Control-Allow-Credentials'] = 'true';
        }
      }

      this.response.writeHead(200, headers);
      this.response.write(data);
      this.log.debug(this.name + ' writing', data);
    };
  });

var server = new Server(config, io);
