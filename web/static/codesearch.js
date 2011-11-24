"use strict";
var Codesearch = function() {
  var MAX_RECONNECT_INTERVAL = 1000*60*1;
  function render_match(match) {
    var h = new HTMLFactory();
    var pieces = [match.line.substring(0, match.bounds[0]),
                  match.line.substring(match.bounds[0], match.bounds[1]),
                  match.line.substring(match.bounds[1])];
    var i;
    var ctx_before = [], ctx_after = [];
    for (i = 0; i < match.context_before.length; i ++) {
      ctx_before.unshift(h.div([
                                 h.span({cls: 'lno'}, [match.lno - i - 1, ":"]),
                                 match.context_before[i]
                               ]));
    }
    for (i = 0; i < match.context_after.length; i ++) {
      ctx_after.push(h.div([
                             h.span({cls: 'lno'}, [match.lno + i + 1, ":"]),
                             match.context_after[i]
                           ]));
    }
    return h.div({cls: 'match'},
                 [
                   h.div({cls: 'label'}, [match.file]),
                   h.div({cls: 'contents'},
                         [
                           ctx_before,
                           h.div({cls: 'matchline'},
                                 [
                                   h.span({cls: 'lno'}, [match.lno + ":"]),
                                   pieces[0],
                                   h.span({cls: 'matchstr'}, [pieces[1]]),
                                   pieces[2]
                                 ]),
                           ctx_after])]);
  }
  function connectFailedMiddleware(cb) {
    return function (remote, client) {
      var timer = setTimeout(function() {
                               client.socketio.disconnect();
                               cb();
                             }, 2000);
      client.on('remote', function() {
                  clearTimeout(timer);
                });
    }
  };
  return {
    remote: null,
    displaying: null,
    results: 0,
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
      DNode({ error: Codesearch.error,
              match: Codesearch.match,
              search_done: Codesearch.search_done,
            }).use(
              connectFailedMiddleware(Codesearch.disconnected)
            ).connect(
              function (remote, conn) {
                Codesearch.remote = remote;
                conn.on('end', Codesearch.disconnected);
                Codesearch.reconnect_interval = 50;
              },
              {
                transports: ["htmlfile", "xhr-polling", "jsonp-polling"]
              });
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
      if (search === Codesearch.input.val()) {
        Codesearch.show_error(error);
      }
    },
    show_error: function (error) {
      $('#errortext').text(error);
      $('#regex-error').show();
    },
    hide_error: function (){
      $('#regex-error').hide();
    },
    match: function(search, match) {
      Codesearch.handle_result(search);
      Codesearch.results++;
      $('#results').append(render_match(match));
      $('#numresults').text(Codesearch.results);
      $('#countarea').show();
    },
    search_done: function(search, time) {
      Codesearch.handle_result(search);
      $('#countarea').show();
      $('#searchtime').text((time/1000) + "s");
      $('#searchtimebox').show();
    },
    handle_result: function(search) {
      Codesearch.hide_error();
      if (search != Codesearch.displaying) {
        $('#numresults').text('0');
        $('#results').empty();
        $('#searchtimebox').hide();
        $('#countarea').hide();
        Codesearch.displaying = search;
        Codesearch.results = 0;
      }
    }
  };
}();
$(document).ready(Codesearch.onload);
