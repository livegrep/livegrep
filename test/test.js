var Codesearch = require('../web/codesearch.js'),
    fs         = require('fs'),
    path       = require('path');

var ITERATIONS = 10;

var REPO = process.argv[2] || '/home/nelhage/code/linux-2.6';

var queries = fs.readFileSync(path.join(__dirname, 'testcases'), 'utf8').split(/\n/);

var cs = new Codesearch(REPO, [], {
                          args: process.argv.slice(3)
                        });
var times = { };

function loop(i) {
    if (i == queries.length) {
        if (ITERATIONS == 0) {
            done();
        } else {
            ITERATIONS--;
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

function done() {
    var results = [];
    for (q in times) {
        results.push([q, times[q], average(times[q], 'time')]);
    }
    results.sort(function (a,b) {
                     return b[2] - a[2]
                 });
    console.log("*** RESULTS ***")
    results.forEach(function (r) {
        var matches = r[1].map(function (f) { return f.nmatch });
        var min_match = Math.min.apply(Math, matches);
        var max_match = Math.min.apply(Math, matches);
        console.log("[%s]: %ss (re2: %s, git: %s) [%d, %d]",
                    r[0], Math.round(r[2])/1000,
                    Math.round(average(r[1], 're2_time'))/1000,
                    Math.round(average(r[1], 'git_time'))/1000,
                    min_match, max_match);
    });
    process.exit(0);
}

cs.once('ready', function() {
            console.log("Begin searching...");
            loop(0);
        });
