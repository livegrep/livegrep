$ = require('jquery');
_ = require('underscore');
var CodeSearchConstant = require("codesearch/codesearch_constants.js");

"use strict";
var Codesearch = function() {
  return {
    delegate: null,
    retry_time: 50,
    next_search: null,
    in_flight: null,
    connect: function(delegate) {
      if (delegate !== undefined)
        Codesearch.delegate = delegate;
      if (Codesearch.delegate.on_connect)
        setTimeout(Codesearch.delegate.on_connect, 0)
    },
    new_search: function(opts) {
      Codesearch.next_search = opts;
      if (Codesearch.in_flight == null)
        Codesearch.dispatch()
    },
    dispatch: function() {
      if (!Codesearch.next_search)
        return;
      Codesearch.in_flight = Codesearch.next_search;
      Codesearch.next_search = null;

      var opts = Codesearch.in_flight;

      var url = "/api/v1/search/";
      if ('backend' in opts) {
        url = url + opts.backend;
      }
      var q = {
        q: opts.q,
        fold_case: opts.fold_case,
        regex: opts.regex,
        repo: opts.repo
      };

      var xhr = $.ajax({
        method: 'POST',
        url: url,
        data: q,
        dataType: "json",
      });
      var start = new Date();
      xhr.done(function (data) {
        var elapsed = new Date() - start;
        data.results.forEach(function (r) {
          Codesearch.delegate.match(opts.id, r);
        });
        data.file_results.forEach(function (r) {
          Codesearch.delegate.file_match(opts.id, r);
        });
        Codesearch.delegate.search_done(opts.id, elapsed, data.search_type, data.info.why);
      });
      xhr.fail(function(data) {
        window._err = data;
        if (data.status >= 400 && data.status < 500) {
          var err = JSON.parse(data.responseText);
          Codesearch.delegate.error(opts.id, err.error.message);
        } else {
          var message = "Cannot connect to server";
          if (data.status) {
            message = "Bad response " + data.status + " from server";
          }
          Codesearch.delegate.error(opts.id, message);
          console.log("server error", data.status, data.responseText);
        }
      });
      xhr.always(function() {
        Codesearch.in_flight = null;
        setTimeout(Codesearch.dispatch, 0);
        Codesearch.setRecentSearchHistory();
      });
    },
    setRecentSearchHistory: function () {
      Codesearch.clear_timer();
      // Timeout ensures only the intended search text is recorded
      Codesearch.timer = setTimeout(function () { 
        if (window.location.search) { 
          var recentSearchesString = localStorage.getItem(CodeSearchConstant.LIVEGREP_STORAGE_KEY);
          var recentSearches = [];
          if (recentSearchesString) { 
            recentSearches = recentSearchesString.split(CodeSearchConstant.RECENT_SEARCHES_DELIMITER);
          }

          const query = window.location.pathname + window.location.search;
          if (recentSearches.indexOf(query) < 0) { 
            if (recentSearches.length >= 5)
              recentSearches.shift();
            recentSearches.push(query);
            localStorage.setItem(CodeSearchConstant.LIVEGREP_STORAGE_KEY, recentSearches.join(CodeSearchConstant.RECENT_SEARCHES_DELIMITER));
          }
        }
      }, 300)
    },
    clear_timer: function () {
      if (Codesearch.timer) {
        clearTimeout(Codesearch.timer);
        Codesearch.timer = null;
      }
    }
  };
}();

module.exports = {
  Codesearch: Codesearch
}
