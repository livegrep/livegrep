"use strict";
var Codesearch = function() {
  return {
    onload: function() {
      Codesearch.input = $('#searchbox');
      Codesearch.input.keydown(Codesearch.keypress);
    },
    keypress: function() {
      setTimeout(Codesearch.newsearch, 0);
    },
    newsearch: function() {
      console.log("Search: " + Codesearch.input.val());
    }
  };
}();
$(document).ready(Codesearch.onload);
