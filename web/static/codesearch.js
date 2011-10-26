"use strict";
var Codesearch = function() {
  return {
    remote: null,
    onload: function() {
      Codesearch.input = $('#searchbox');
      Codesearch.input.keydown(Codesearch.keypress);
      DNode.connect(function (remote) {
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
    }
  };
}();
$(document).ready(Codesearch.onload);
