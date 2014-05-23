"use strict";
var Codesearch = function() {
  return {
    socket: null,
    delegate: null,
    retry_time: 50,
    connect: function(delegate) {
      if (delegate !== undefined)
        Codesearch.delegate = delegate;
      if (Codesearch.socket !== null)
        return;
      console.log("Attempting to connect via websocket...");

      var proto = window.location.protocol.replace('http', 'ws');
      var socket = new WebSocket(proto + "//" + window.location.host + "/socket");
      socket.onmessage = Codesearch.handle_frame
      socket.onopen = function() {
        Codesearch.socket = socket;
        if (Codesearch.delegate.on_connect)
          Codesearch.delegate.on_connect();
        console.log("Connected!")
        Codesearch.retry_time = 50;
      }
      socket.onerror = Codesearch.socket_error;
      socket.onclose = Codesearch.socket_close;
    },
    new_search: function(opts) {
      if (Codesearch.socket !== null)
        Codesearch.socket.send(JSON.stringify({opcode: "query", body: opts}));
    },
    handle_frame: function(frame) {
      var op = JSON.parse(frame.data);
      if (op.body.opcode == "error") {
        console.log("in-band error: ", op.error)
      } else if (op.opcode == 'result') {
        Codesearch.delegate.match(op.body.id, op.body.result);
      } else if (op.opcode == 'search_done') {
        Codesearch.delegate.search_done(op.body.id, op.body.time, op.body.stats.why);
      } else if (op.opcode == 'query_error') {
        Codesearch.delegate.error(op.body.id, op.body.error);
      } else {
        console.log("unknown opcode: ", op.opcode)
      }
    },
    socket_error: function(err) {
      console.log("Socket error: ", err);
    },
    socket_close: function() {
      Codesearch.socket = null;
      Codesearch.retry_time = Math.min(600 * 1000, Math.round(Codesearch.retry_time * 1.5));
      console.log("Socket closed. Retry in " + Codesearch.retry_time + "ms.")
      setTimeout(Codesearch.connect, Codesearch.retry_time)
    }
  };
}();
