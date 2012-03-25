#!/usr/bin/env node
var express = require('express'),
    path    = require('path'),
    parseopt= require('parseopt'),
    handlebars = require('handlebars'),
    log4js  = require('log4js'),
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

app.configure(
  function() {
    app.register('.html', require('handlebars'));
    app.set("view options", { layout: false });
    app.set('view engine', 'html');
    app.set('views', path.join(__dirname, 'templates'));
    app.use(express.static(path.join(__dirname, 'htdocs')));
  });

app.get('/', function (req, res) {res.render('index');});
app.get('/about', function (req, res) {res.render('about');});

app.listen(8910);
console.log("http://localhost:8910");

var io = require('socket.io').listen(app, {
                                       logger: log4js.getLogger('socket.io'),
                                       'log level': log4js.levels.INFO
                                     });
io.configure(
  function() {
    io.enable('browser client gzip');
  });

var server = new Server(config, io);
