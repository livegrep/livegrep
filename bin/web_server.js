#!/usr/bin/env node
var express = require('express'),
    http    = require('http'),
    hbs     = require('hbs'),
    extras  = require('express-extras'),
    path    = require('path'),
    parseopt= require('parseopt'),
    log4js  = require('log4js'),
    email   = require('emailjs'),
    util    = require('util'),
    Server  = require('../js/appserver.js'),
    config  = require('../js/config.js');

function shorten(ref) {
  var match = /^refs\/(tags|branches)\/(.*)/.exec(ref);
  if (match)
    return match[2];
  return ref;
}

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

var app = express();
var logger = log4js.getLogger('web');

app.configure(
  function() {
    app.use(extras.fixIP());
    app.use(log4js.connectLogger(logger, {
                                   level: log4js.levels.INFO,
                                   format: function (req, res, fmt) {
                                     return '' + req.ip + fmt(' [:date] :method :url');
                                   }
                                 }));
    app.engine('.html', hbs.__express);
    app.engine('.xml', hbs.__express);
    app.set('view engine', 'html');
    app.set('view options', {
              production: opts.options.production
            });
    app.set('views', path.join(__dirname, '../web/templates'));
    app.use(express.bodyParser());
    app.use(express.static(path.join(__dirname, '../web/htdocs')));
    if (opts.options.production) {
      app.enable('trust proxy');
    }
    hbs.handlebars.registerHelper('json', function (data) {
      return new hbs.handlebars.SafeString(JSON.stringify(data).replace(/<\/script>/g, '<\\/script>'));
    });
  });

app.get('/', function (req, res) {res.redirect('/search');});
function searchHandler (req, res) {
  var repo_map = {};
  Object.keys(config.BACKENDS).forEach(function (name) {
    var backend = config.BACKENDS[name];
    repo_map[name] = {};
    backend.repos.forEach(function (repo) {
      repo_map[name][repo.name] = repo.github;
    });
  });
  res.render('index',
             {
               js: true,
               title: 'search',
               repos: Object.keys(config.BACKENDS).map(function (name) {
                 var backend = config.BACKENDS[name];
                 return {
                   name: name,
                   pretty: backend.pretty_name
                 };
               }),
               multi_repo: Object.keys(config.BACKENDS).length > 1,
               repo_name: (config.BACKENDS['']||{}).pretty_name,
               github_repos: repo_map,
               production: opts.options.production
             });
}
app.get('/search', searchHandler);
app.get('/search/:backend', searchHandler);
app.get('/about', function (req, res) {
          res.render('about',
                     {
                       title: 'about',
                       production: opts.options.production
                     });
        });
app.get('/opensearch.xml', function (req, res) {
          var backend = config.BACKENDS[Object.keys(config.BACKENDS)[0]];
          res.render('opensearch.xml', {
              'layout': false,
              'baseURL': req.protocol + "://" + req.get('Host') + "/",
              'backend': backend
          });
});
function send_feedback(data, cb) {
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
                  data.remoteAddress,
                  data.session,
                  data.text
                )
              }, function (err, message) {
                if (err) {
                  console.log("Error sending email!", err);
                  cb(err);
                } else {
                  console.log("Email sent!");
                  cb();
                }
              });
  } else {
    process.nextTick(cb);
  }
}

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

           data.remoteAddress = req.connection.remoteAddress;
           send_feedback(data,
                         function (err) {
                           if (err) {
                             res.send(500);
                           } else {
                             res.send(200);
                           }
                         });
         });

var server = http.createServer(app);

server.listen(config.WEB_PORT);
console.log("Listening on :%d.", config.WEB_PORT);

var io = require('socket.io').listen(server, {
                                       logger: log4js.getLogger('socket.io'),
                                       'log level': log4js.levels.INFO
                                     });
if (config.SOCKET_IO_TRANSPORTS)
  io.set('transports', config.SOCKET_IO_TRANSPORTS);

var server = new Server(config, io);
