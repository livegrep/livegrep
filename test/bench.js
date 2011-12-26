var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path'),
    common     = require('./common.js');

common.parser.add('--dump-stats', {type: 'string', target: 'dump_stats'});
common.parser.add('--iterations', {type: 'int', default: 10});
var options = common.parseopts();
var queries = common.load_queries();
var cs = common.get_codesearch(['--timeout=0']);

var times = { };

var ITERATIONS = options.iterations;

function loop(i) {
  if (i == queries.length) {
    if (--ITERATIONS == 0) {
      done();
    } else {
      loop(0);
    }
    return;
  }
  var q = queries[i];
  var start = new Date();
  var search = cs.search(q);
  var results = 0;
  search.on('match', function () {
              results++;
            })
  search.on('done',
            function (stats) {
              var end = new Date();
              var time = +(end - start);
              if (!(q in times))
                times[q] = [];
              stats.time = time;
              stats.nmatch = results;
              times[q].push(stats);
              cs.once('ready', function() {
                        loop(i+1);
                      });
            });
}

function average(l, field) {
  var sum = 0;
  l.forEach(function (e) {sum += e[field];});
  return sum / l.length;
}

function rpad(str, len, chr) {
  if (chr === undefined)
    chr = ' '
  str = '' + str;
  while (str.length < len)
    str += chr;
  return str;
}

function done() {
  var results;
  if (options.dump_stats)
    fs.writeFileSync(options.dump_stats,
                     JSON.stringify(times))

  results = collate(times);
  print_one(results);

  process.exit(0);
}

function collate(times) {
  var out = [];
  for (q in times) {
    out.push([q, times[q], average(times[q], 'time')]);
  }
  return out;
}

function fmt(re) {
  var WIDTH = 20;
  if (re.length < WIDTH) {
    re = rpad(re, WIDTH);
  }
  if (re.length > WIDTH) {
    var start = re.substr(0, WIDTH / 2);
    var end   = re.substring(re.length - (WIDTH - start.length - 3));
    re = start + '...' + end;
  }
  return re;
}

function num(n) {
  n = Math.round(n);
  var str;
  if (n === 0.0)
    str = '0.0'
  else
    str = ''+(n/1000);
  return rpad(str, 6, '0')
}

function print_one(results) {
  console.log("*** RESULTS ***")

  results.sort(function (a,b) {
                 return b[2] - a[2]
               });

  results.forEach(function (r) {
                    var min_time = Math.min.apply(
                      Math, r[1].map(function(r) {return r.time}));
                    var max_time = Math.max.apply(
                      Math, r[1].map(function(r) {return r.time}));
                    function time(name) {
                      var tm = Math.round(average(r[1], name + '_time'));
                      return num(tm);
                    }

                    console.log("[%s]: %s<%ss (re2: %s, index: %s)",
                                fmt(r[0]),
                                num(min_time),
                                num(r[2]),
                                time('re2'), time('index'));
                  });
}

cs.once('ready', function() {
          console.log("Begin searching...");
          loop(0);
        });
