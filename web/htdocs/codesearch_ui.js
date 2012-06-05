$(function() {

"use strict";
var Match = Backbone.Model.extend({
  url: function() {
    return "https://github.com/torvalds/linux/blob/" + shorten(this.ref) +
      "/" + this.get('path').path + "#L" + this.get('context').lno;
  }
});

function shorten(ref) {
  var match = /^refs\/(tags|branches)\/(.*)/.exec(ref);
  if (match)
    return match[2];
  return ref;
}

var MatchView = Backbone.View.extend({
  tagName: 'div',
  render: function() {
    var div = this._render();
    this.$el.empty();
    this.$el.append(div);
    return this;
  },
  _render: function() {
    var h = new HTMLFactory();
    var ctx = this.model.get('context');
    var i;
    var ctx_before = [], ctx_after = [];
    for (i = 0; i < ctx.context_before.length; i ++) {
      ctx_before.unshift(h.div([
                                 h.span({cls: 'lno'}, [ctx.lno - i - 1, ":"]),
                                 ctx.context_before[i]
                               ]));
    }
    for (i = 0; i < ctx.context_after.length; i ++) {
      ctx_after.push(h.div([
                             h.span({cls: 'lno'}, [ctx.lno + i + 1, ":"]),
                             ctx.context_after[i]
                           ]));
    }
    var line = this.model.get('line');
    var bounds = this.model.get('bounds');
    var pieces = [line.substring(0, bounds[0]),
                  line.substring(bounds[0], bounds[1]),
                  line.substring(bounds[1])];

    return h.div({cls: 'match'},
                 [
                   h.div({},
                         [h.span({cls: 'label'},
                                 [
                                   h.a({
                                         href: this.model.url()
                                       },
                                   [ shorten(this.model.get('path').ref), ":",
                                     this.model.get('path').path])])
                         ]),
                   h.div({cls: 'contents'},
                         [
                           ctx_before,
                           h.div({cls: 'matchline'},
                                 [
                                   h.span({cls: 'lno'}, [ctx.lno + ":"]),
                                   pieces[0],
                                   h.span({cls: 'matchstr'}, [pieces[1]]),
                                   pieces[2]
                                 ]),
                           ctx_after])]);
  }
});

var MatchSet = Backbone.Collection.extend({
  model: Match
});

var ResultView = Backbone.View.extend({
  el: $('#resultbox'),
  initialize: function() {
    CodesearchUI.matches.bind('add', this.add_one, this);
    CodesearchUI.matches.bind('reset', this.add_all, this);

    this.results = this.$('#results');
  },
  add_one: function(model) {
    var view = new MatchView({model: model});
    this.results.append(view.render().el);
  },
  add_all: function(mode) {
    this.results.empty();
    CodesearchUI.matches.each(this.add_one);
  }
});

var CodesearchUI = function() {
  return {
    displaying: null,
    results: 0,
    search_id: 0,
    search_map: {},
    matches: new MatchSet(),
    view: null,
    onload: function() {
      if (CodesearchUI.input)
        return;

      CodesearchUI.view = new ResultView();

      CodesearchUI.input      = $('#searchbox');
      CodesearchUI.input_file = $('#filebox');
      var parms = CodesearchUI.parse_query_params();
      if (parms.q)
        CodesearchUI.input.val(parms.q);
      if (parms.file)
        CodesearchUI.input_file.val(parms.file);

      CodesearchUI.input.keydown(CodesearchUI.keypress);
      CodesearchUI.input_file.keydown(CodesearchUI.keypress);
      CodesearchUI.input.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input_file.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input.focus();

      Codesearch.connect(CodesearchUI);
    },
    parse_query_params: function() {
      var urlParams = {};
      var e,
          a = /\+/g,
          r = /([^&=]+)=?([^&]*)/g,
          d = function (s) { return decodeURIComponent(s.replace(a, " ")); },
          q = window.location.search.substring(1);

      while (e = r.exec(q))
        urlParams[d(e[1])] = d(e[2]);
      return urlParams;
    },
    on_connect: function() {
      CodesearchUI.newsearch();
    },
    keypress: function() {
      setTimeout(CodesearchUI.newsearch, 0);
    },
    newsearch: function() {
      CodesearchUI.search_map[++CodesearchUI.search_id] = {
        q: CodesearchUI.input.val(),
        file: CodesearchUI.input_file.val()
      };
      Codesearch.new_search(
        CodesearchUI.input.val(),
        CodesearchUI.input_file.val(),
        CodesearchUI.search_id);
      if (!CodesearchUI.input.val().length) {
        CodesearchUI.clear();
        CodesearchUI.displaying = CodesearchUI.search_id;
        CodesearchUI.update_url({});
      }
    },
    clear: function() {
      CodesearchUI.hide_error();
      $('#numresults').val('');
      CodesearchUI.matches.reset();
      $('#searchtimebox').hide();
      $('#resultarea').hide();
    },
    error: function(search, error) {
      if (search === CodesearchUI.search_id) {
        CodesearchUI.show_error(error);
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
      CodesearchUI.handle_result(search);
      CodesearchUI.results++;
      CodesearchUI.matches.add({
                                 line: match.line,
                                 bounds: match.bounds,
                                 context: match.contexts[0],
                                 path: match.contexts[0].paths[0],
                               });
      $('#numresults').text(CodesearchUI.results);
    },
    search_done: function(search, time, why) {
      CodesearchUI.handle_result(search);
      if (why === 'limit')
        $('#numresults').append('+');
      $('#searchtime').text((time/1000) + "s");
      $('#searchtimebox').show();
    },
    handle_result: function(search) {
      CodesearchUI.hide_error();
      if (search <= CodesearchUI.displaying)
        return;

      for (var k in CodesearchUI.search_map) {
        if (k < search)
          delete CodesearchUI.search_map[k];
      }
      CodesearchUI.clear();
      $('#numresults').text('0');
      $('#resultarea').show();
      CodesearchUI.displaying = search;
      CodesearchUI.results = 0;
      CodesearchUI.update_url(CodesearchUI.search_map[search]);
    },
    update_url: function (q) {
      if (!q.q)    delete q.q;
      if (!q.file) delete q.file;
      var url = '/search?' + $.param(q);
      if (history.replaceState) {
        history.replaceState(null, '', url);
      }
      $('#permalink').attr('href', url);
    }
  };
}();
CodesearchUI.onload();

});
