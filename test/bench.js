var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path'),
    printf     = require('printf'),
    common     = require('./common.js'),
    stats      = require('../lib/stats.js');

common.parser.add('--dump-stats', {type: 'string', target: 'dump_stats'});
common.parser.add('--load-stats', {type: 'string', target: 'load_stats'});
common.parser.add('--compare',    {type: 'string'});
common.parser.add('--iterations', {type: 'int', default: 10});
var options = common.parseopts();
var queries = common.load_queries();
var cs = common.get_codesearch(['--timeout=0']).connect();

var times = { };
var cmp_times = null;

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
  return stats.mean(l.map(function (r) {return r[field];}));
}

function rpad(str, len, chr) {
  if (chr === undefined)
    chr = ' '
  str = '' + str;
  while (str.length < len)
    str += chr;
  return str;
}

function lpad(str, len, chr) {
  if (chr === undefined)
    chr = ' '
  str = '' + str;
  while (str.length < len)
    str = chr + str;
  return str;
}

function done() {
  var results;
  if (options.dump_stats)
    fs.writeFileSync(options.dump_stats,
                     JSON.stringify(times))
  if (options.compare) {
    compare(cmp_times, times);
  } else {
    results = collate(times);
    print_one(results);
  }

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

function pct(n) {
  n = Math.round(100*n);
  if (n >= 0)
    n = '+' + n;
  else
    n = '' + n;
  return lpad(n, 4, ' ') + '%';
}

function compare(prev, cur) {
  var cmp = [];
  Object.keys(cur).forEach(
    function (re) {
      if (!prev.hasOwnProperty(re))
        return;
      var prev_mean = average(prev[re], 'time');
      var cur_mean  = average(cur[re], 'time');
      cmp.push({
                 re: re,
                 prev: prev[re],
                 prev_mean: prev_mean,
                 prev_stdev: stats.stdev(prev[re].map(function (e) {return e.time;})),
                 cur: cur[re],
                 cur_mean: cur_mean,
                 cur_stdev: stats.stdev(cur[re].map(function (e) {return e.time;})),
                 delta: (prev_mean === 0.0) ? 0 : (cur_mean - prev_mean)/prev_mean,
               });
    })
  cmp.sort(function (a,b) {return a.delta - b.delta;});

  print_compare(cmp);
}

function print_compare(cmp) {
  console.log("Results VERSUS %s", options.compare);
  cmp.forEach(
    function (r) {
      printf(process.stdout,
             "[%s]: %4.3f/%4.3f (%+3d%% / %+4.3fσ)\n",
             fmt(r.re),
             r.prev_mean/1000, r.cur_mean/1000,
             100*r.delta,
             (r.cur_mean - r.prev_mean) / r.prev_stdev);
    });
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
                      return average(r[1], name + '_time')/1000;
                    }

                    printf(process.stdout,
                           "[%s]: %4.3f<%4.3fs (re2: %4.3f, index: %4.3f, analyze: %4.3f)\n",
                           fmt(r[0]), min_time/1000, r[2]/1000,
                           time('re2'), time('index'), time('analyze'));
                  });
}


if (options.compare) {
  try {
    cmp_times = JSON.parse(fs.readFileSync(options.compare));
  } catch(e) {
    console.error("Unable to load data for comparison:");
    console.error(" %s", e);
    process.exit(1);
  }
}

if (options.load_stats) {
  try {
    times = JSON.parse(fs.readFileSync(options.load_stats));
  } catch(e) {
    console.error("Unable to load data:");
    console.error(" %s", e);
    process.exit(1);
  }
  done();
} else {
  cs.once('ready', function() {
            console.log("Begin searching...");
            loop(0);
          });
}
