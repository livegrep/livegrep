#!/usr/bin/env node
var express = require('express'),
    path    = require('path'),
    parseopt= require('parseopt'),
    handlebars = require('handlebars'),
    log4js  = require('log4js'),
    email   = require('emailjs'),
    util    = require('util'),
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
      },
      {
        name: "--production",
        default: false,
        type: 'flag',
        help: 'Enable options for a production deployment.'
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

var smtp = null;
if (config.SMTP_CONFIG) {
  smtp = email.server.connect(config.SMTP_CONFIG);
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
    app.set('view engine', 'html');
    app.set('view options', {
              production: opts.options.production
            });
    app.set('views', path.join(__dirname, 'templates'));
    app.use(express.bodyParser());
    app.use(express.static(path.join(__dirname, 'htdocs')));
  });

app.get('/', function (req, res) {
          res.render('index',
                     {
                       js: true,
                       title: 'search'
                     });
        });
app.get('/about', function (req, res) {
          res.render('about',
                     {
                       title: 'about'
                     });
        });
app.post('/feedback', function (req, res) {
           console.log("FEEDBACK", req.body);
           if (!('data' in req.body)) {
             res.send(400);
             return;
           }
           var data;
           try {
             data = JSON.parse(req.body.data);
           } catch(e) {
             console.log("Feedback error: %s", e);
             res.send(400);
             return;
           }

           if (!data.email && !data.text) {
             console.log("Empty feedback: %j", data);
             res.send(200);
             return;
           }

           if (smtp) {
             smtp.send({
                         to: "Nelson Elhage <feedback@livegrep.com>",
                         from: "Codesearch <mailer@livegrep.com",
                         subject: "Feedback from codesearch!",
                         text: util.format(
                           "Codesearch feedback from: %s \n" +
                             "IP: %s\n" +
                             "Session: %s\n\n" +
                             "%s",
                           data.email,
                           req.connection.remoteAddress,
                           data.session,
                           data.text
                         )
                       }, function (err, message) {
                         if (err) {
                           console.log("Error sending email!", err);
                           res.send(500);
                         } else {
                           console.log("Email sent!");
                           res.send(200);
                         }
                       });
           } else {
             res.send(200);
           }
         });

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
