"use strict";
var Benchmark = function() {
  var WINDOW = 5000;
  var queries = [
    "____do",
    "errno\\W",
    "kmalloc\\(",
    "printk\\(",
    "^(\\s.*\\S)?kmalloc\\s*\\(",
    "^(\\s.*\\S)?printk\\s*\\(",
    "^(\\s.*\\S)?acct_",
    ".",
    "^$",
    "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}",
  ];

  function max_time() {
    var max = {time: 0};
    Benchmark.responses.forEach(function (r) {
                                  if (r.time > max.time)
                                    max = r
                                });
    return max;
  }

  function render() {
    var now = new Date();
    var ms = +(now - Benchmark.start_time)
    $("#val_time").text(ms);
    $("#val_searches").text(Benchmark.searches);
    $("#val_errors").text(Benchmark.errors);
    $("#val_window").text(1000 * Benchmark.responses.length /
        (now - Benchmark.responses[0].end));
    var max = max_time();
    $("#val_max").text(max.time);
    $("#val_serv_max").text(max.serv_time);
  }

  function done(search, error, time) {
    var now = new Date();
    Benchmark.searches++;
    if (error)
      Benchmark.errors++;
    Benchmark.responses.push({
                               end: now,
                               time: now - Benchmark.search_start[search],
                               serv_time: time
                             });
    while ((now - Benchmark.responses[0].end) > WINDOW)
      Benchmark.responses.shift(1);
    render();
    Object.keys(Benchmark.search_start).forEach(
      function (n) {
        if (+n < search)
          delete Benchmark.search_start[n];
      });
  }

  function loop(i) {
    if (i === queries.length)
      i = 0;
    Benchmark.search_start[++Benchmark.search_id] = new Date();
    Codesearch.new_search(queries[i], Benchmark.search_id);
    Benchmark.timer = setTimeout(function() {loop(i+1)}, 10);
  }

  return {
    start_time: 0,
    search_start: {},
    search_id: 0,
    searches: 0,
    errors: 0,
    responses: [],

    timer: undefined,
    onload: function() {
      Codesearch.connect(Benchmark);
    },
    regex_error: function(search, err) {
      done(search, true, 0);
    },
    match: function(search, match) {
    },
    search_done: function(search, time, why) {
      done(search, false, time);
    },
    on_connect: function() {
      if (Benchmark.timer === undefined)
        Benchmark.start();
    },
    start: function() {
      Benchmark.start_time = new Date();
      Benchmark.searches = 0;
      Benchmark.errors = 0;
      loop(0);
    },
    stop: function() {
      if (Benchmark.timer) {
        clearTimeout(Benchmark.timer);
        Benchmark.timer = null;
      }
    }
  }
}();

$(document).ready(Benchmark.onload);
