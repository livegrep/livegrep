"use strict";
var Codesearch = function() {
  var MAX_RECONNECT_INTERVAL = 1000*60*1;
  return {
    remote: null,
    displaying: null,
    reconnect_interval: 50,
    onload: function() {
      Codesearch.input = $('#searchbox');
      Codesearch.input.keydown(Codesearch.keypress);
      Codesearch.connect()
    },
    connect: function() {
      if (Codesearch.remote !== null)
        return;
      console.log("Connecting...");
      Codesearch.remote = null;
      var timer = setTimeout(Codesearch.connect_failed, 500);
      DNode({ error: Codesearch.regex_error,
              match: Codesearch.match,
              search_done: Codesearch.search_done,
            }).connect(function (remote, conn) {
                         Codesearch.remote = remote;
                         conn.on('end', Codesearch.disconnected);
                         clearTimeout(timer);
                         Codesearch.reconnect_interval = 50;
                    });
    },
    connect_failed: function() {
      Codesearch.disconnected();
    },
    disconnected: function() {
      console.log("Reconnecting in " + Codesearch.reconnect_interval)
      Codesearch.remote = null;
      setTimeout(Codesearch.connect, Codesearch.reconnect_interval);
      Codesearch.reconnect_interval *= 2;
      if (Codesearch.reconnect_interval > MAX_RECONNECT_INTERVAL)
        Codesearch.reconnect_interval = MAX_RECONNECT_INTERVAL;
    },
    keypress: function() {
      setTimeout(Codesearch.newsearch, 0);
    },
    newsearch: function() {
      if (Codesearch.remote !== null)
        Codesearch.remote.new_search(Codesearch.input.val());
    },
    error: function(search, error) {
    },
    match: function(search, match) {
      Codesearch.handle_result(search);
      var li = document.createElement('li');
      var pre = document.createElement('pre');
      pre.appendChild(document.createTextNode(
                      match.file + ":" + match.lno + ":" + match.line));
      li.appendChild(pre);
      $('#results').append(li);
    },
    search_done: function(search) {
      Codesearch.handle_result(search);
    },
    handle_result: function(search) {
      if (search != Codesearch.displaying) {
        $('#results').children().remove();
        Codesearch.displaying = search;
      }
    }
  };
}();
$(document).ready(Codesearch.onload);
