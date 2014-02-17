"use strict";
var Codesearch = function() {
  return {
    socket: null,
    delegate: null,
    connect: function(delegate) {
      Codesearch.delegate = delegate;
      if (Codesearch.socket !== null)
        return;

      var socket = new WebSocket("ws://" + window.location.host + "/socket")
      socket.onmessage = Codesearch.handle_frame
      socket.onopen = function() {
        Codesearch.socket = socket;
        if (Codesearch.delegate.on_connect)
          Codesearch.delegate.on_connect();
      }
      socket.onerror = Codesearch.socket_error
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
        Codesearch.delegate.search_done(op.body.id, 0, op.body.why);
      } else if (op.opcode == 'query_error') {
        Codesearch.delegate.error(op.body.id, op.body.error);
      } else {
        console.log("unknown opcode: ", op.opcode)
      }
    },
    socket_error: function(err) {
      console.log("Socket error: ", err);
    }
  };
}();
