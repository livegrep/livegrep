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

  function render() {
    var now = new Date();
    var ms = +(now - Benchmark.start_time)
    $("#val_time").text(ms);
    $("#val_searches").text(Benchmark.searches);
    $("#val_errors").text(Benchmark.errors);
    $("#val_qps").text(1000 * Benchmark.searches / ms);
    $("#val_window").text(1000 * Benchmark.responses.length /
        (now - Benchmark.responses[0]));
  }

  function done(error) {
    Benchmark.searches++;
    if (error)
      Benchmark.errors++;
    Benchmark.responses.push(new Date());
    var now = new Date();
    while ((now - Benchmark.responses[0]) > WINDOW)
      Benchmark.responses.shift(1);
    render();
  }

  function loop(i) {
    if (i === queries.length)
      i = 0;
    Codesearch.new_search(queries[i]);
    Benchmark.timer = setTimeout(function() {loop(i+1)}, 10);
  }

  function start() {
    Benchmark.start_time = new Date();
    Benchmark.searches = 0;
    Benchmark.errors = 0;
    loop(0);
  }

  return {
    start_time: 0,
    searches: 0,
    errors: 0,
    responses: [],

    timer: undefined,
    onload: function() {
      Codesearch.connect(Benchmark);
    },
    error: function(search, err) {
      done(true);
    },
    match: function(search, match) {
    },
    search_done: function(search, time, why) {
      done(false);
    },
    on_connect: function() {
      if (Benchmark.timer === undefined)
        start();
    },
    playpause: function() {
      var btn = $('#playpause');
      if (Benchmark.timer) {
        clearTimeout(Benchmark.timer);
        Benchmark.timer = null;
        btn.text("start")
      } else {
        start();
        btn.text("stop")
      }
    }
  }
}();

$(document).ready(Benchmark.onload);
