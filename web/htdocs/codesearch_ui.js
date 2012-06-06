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

var SearchState = Backbone.Model.extend({
  defaults: function() {
    return {
      displaying: null,
      error: null,
      time: null,
      why: null
    };
  },

  initialize: function() {
    this.search_map = {};
    this.matches = new MatchSet();
    this.search_id = 0;
    this.on('change:displaying', this.new_search, this);
  },

  new_search: function() {
    this.set({
        error: null,
        time: null,
        why: null
    });
    this.matches.reset();
    for (var k in this.search_map) {
      if (k < this.get('displaying'))
        delete this.search_map[k];
    }
  },

  next_id: function() {
    return ++this.search_id;
  },

  dispatch: function (q, file) {
    var id = this.next_id();
    this.search_map[id] = {q: q, file: file};
    if (!q.length)
      this.set('displaying', id);
    return id;
  },

  url: function() {
    var q = {};
    var current = this.search_map[this.get('displaying')];
    if (!current)
      return '/search';

    if (current.q)    q.q = current.q;
    if (current.file) q.file = current.file;
    return '/search?' + $.param(q);
  },

  handle_error: function (search, error) {
    if (search === this.search_id) {
      this.set('displaying', search);
      this.set('error', error);
    }
  },

  handle_match: function (search, match) {
    if (search < this.get('displaying'))
      return false;
    this.set('displaying', search);
    this.matches.add({
                       line: match.line,
                       bounds: match.bounds,
                       context: match.contexts[0],
                       path: match.contexts[0].paths[0],
                     });
  },
  handle_done: function (search, time, why) {
    this.set('displaying', search);
    this.set({time: time, why: why});
  }
});

var MatchesView = Backbone.View.extend({
  el: $('#results'),
  initialize: function() {
    this.model.matches.bind('add', this.add_one, this);
    this.model.matches.bind('reset', this.add_all, this);
  },
  add_one: function(model) {
    var view = new MatchView({model: model});
    this.$el.append(view.render().el);
  },
  add_all: function() {
    this.$el.empty();
    this.model.matches.each(this.add_one);
  }
});

var ResultView = Backbone.View.extend({
  el: $('#resultarea'),
  initialize: function() {
    this.matches_view = new MatchesView({model: this.model});
    this.permalink = this.$('#permalink');
    this.results   = this.$('#numresults');
    this.errorbox  = $('#regex-error');
    this.time      = this.$('#searchtime');

    this.model.on('all', this.render, this);
    this.model.matches.on('all', this.render, this);
  },

  render: function() {
    if (this.model.get('error')) {
      this.errorbox.find('#errortext').text(this.model.get('error'));
      this.errorbox.show();
    } else {
      this.errorbox.hide()
    }

    var url = this.model.url();
    this.permalink.attr('href', url);
    if (history.replaceState) {
      history.replaceState(null, '', url);
    }

    if (this.model.search_map[this.model.get('displaying')].q === '' ||
       this.model.get('error')) {
      this.$el.hide();
      return this;
    }

    this.$el.show();

    if (this.model.get('time')) {
      this.$('#searchtimebox').show();
      var time = this.model.get('time');
      this.time.text((time/1000) + "s")
    } else {
      this.$('#searchtimebox').hide();
    }

    var results = '' + this.model.matches.size();
    if (this.model.get('why') === 'limit')
      results = results + '+';
    this.results.text(results);

    return this;
  }
});

var CodesearchUI = function() {
  return {
    state: new SearchState(),
    view: null,
    onload: function() {
      if (CodesearchUI.input)
        return;

      CodesearchUI.view = new ResultView({model: CodesearchUI.state});

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
      var id = CodesearchUI.state.dispatch(CodesearchUI.input.val(),
                                           CodesearchUI.input_file.val());
      Codesearch.new_search(
        CodesearchUI.input.val(),
        CodesearchUI.input_file.val(),
        id);
    },
    error: function(search, error) {
      CodesearchUI.state.handle_error(search, error);
    },
    match: function(search, match) {
      CodesearchUI.state.handle_match(search, match);
    },
    search_done: function(search, time, why) {
      CodesearchUI.state.handle_done(search, time, why);
    },
  };
}();
CodesearchUI.onload();
window.CodesearchUI = CodesearchUI;
});
