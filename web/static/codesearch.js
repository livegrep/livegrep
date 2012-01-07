"use strict";
var Codesearch = function() {
  return {
    remote: null,
    delegate: null,
    connect: function(delegate) {
      if (Codesearch.remote !== null)
        return;
      console.log("Connecting...");
      Codesearch.remote = null;
      Codesearch.delegate = delegate;
      DNode({ error: Codesearch.delegate.error,
              match: Codesearch.delegate.match,
              search_done: Codesearch.delegate.search_done,
            }).connect(
              function (remote, conn) {
                Codesearch.remote = remote;
                if (Codesearch.delegate.on_connect)
                  Codesearch.delegate.on_connect();
              },
              {
                transports: ["htmlfile", "xhr-polling", "jsonp-polling"]
              });
    },
    new_search: function(re) {
      if (Codesearch.remote !== null)
        Codesearch.remote.new_search(re);
    }
  };
}();
