"use strict";
var Codesearch = function() {
  return {
    remote: null,
    displaying: null,
    onload: function() {
      Codesearch.input = $('#searchbox');
      Codesearch.input.keydown(Codesearch.keypress);
      DNode({ error: Codesearch.regex_error,
              match: Codesearch.match,
              search_done: Codesearch.search_done,
            }).connect(function (remote) {
                      Codesearch.remote = remote;
                    }, {
                      reconnect: 100
                    });
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
