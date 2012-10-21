$(function() {
"use strict";

function vercmp(a, b) {
  var re = /^([0-9]*)([^0-9]*)(.*)$/;
  var abits, bbits;
  var anum, bnum;
  while (a.length && b.length) {
    abits = re.exec(a);
    bbits = re.exec(b);
    if ((abits[1] === '') != (bbits[1] === '')) {
      return abits[1] ? -1 : 1;
    }
    if (abits[1] !== '') {
      anum = parseInt(abits[1]);
      bnum = parseInt(bbits[1])
      if (anum !== bnum)
        return anum - bnum;
    }

    if (abits[2] !== bbits[2]) {
      return abits[2] < bbits[2] ? -1 : 1
    }

    a = abits[3];
    b = bbits[3];
  }

  return a.length - b.length;
}

function shorten(ref) {
  var match = /^refs\/(tags|branches)\/(.*)/.exec(ref);
  if (match)
    return match[2];
  match = /^([0-9a-f]{8})[0-9a-f]+$/.exec(ref);
  if (match)
    return match[1];
  return ref;
}

var Match = Backbone.Model.extend({
  initialize: function() {
    this.get('contexts').forEach(function (ctx) {
        ctx.paths.sort(function (a,b) {return vercmp(a.ref, b.ref);})
    });
    this.get('contexts').sort(function (a,b) {
        return vercmp(a.paths[0].ref, b.paths[0].ref);
    });
    this.set({
               context: this.get('contexts')[0],
               path: this.get('contexts')[0].paths[0]
             });
  },
  url: function() {
    return "https://github.com/" + CodesearchUI.github_repo + "/blob/" + shorten(this.get('path').ref) +
      "/" + this.get('path').path + "#L" + this.get('context').lno;
  }
});

var MatchView = Backbone.View.extend({
  tagName: 'div',
  initialize: function() {
    this.model.on('change', this.render, this);
  },
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

    return h.div({cls: 'match'}, [
        h.div({}, [
          h.span({cls: 'label'}, [
            h.a({href: this.model.url()}, [
                  shorten(this.model.get('path').ref),
                  ":",
                  this.model.get('path').path])])]),
        h.div({cls: 'contents'}, [
                ctx_before,
                h.div({cls: 'matchline'}, [
                  h.span({cls: 'lno'}, [ctx.lno + ":"]),
                  pieces[0],
                  h.span({cls: 'matchstr'}, [pieces[1]]),
                  pieces[2]
                ]),
                ctx_after]),
        this.render_contexts(h)]);
  },
  render_contexts: function(h) {
    var self = this;
    if (this.model.get('contexts').length == 1 &&
        this.model.get('context').paths.length == 1)
      return [];
    return h.div({cls: 'contexts'}, [
          h.span({cls: 'label'}, ["Also in:"]),
          h.ul({},
          this.model.get('contexts').map(function (ctx) {
            return h.li(ctx === self.model.get('context') ? {cls: 'selected'} : {}, [
                h.a({href: "#",
                     click: _.bind(self.switch_context, self, ctx)}, [
                shorten(ctx.paths[0].ref)]),
                ctx.paths.length > 1 ? (" +" + (ctx.paths.length - 1) + " identical") : [],
            ]);
          }))])
  },
  switch_context: function(ctx) {
    this.model.set({
                     context: ctx,
                     path: ctx.paths[0]
                   });
    return false;
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
      if (parseInt(k) < this.get('displaying'))
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
                       contexts: match.contexts
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
    this.last_url  = null;

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
    if (this.last_url !== url ) {
      this.permalink.attr('href', url);
      if (history.replaceState) {
        history.replaceState(null, '', url);
      }
      this.last_url = url;
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
    github_repo: ""
  };
}();
CodesearchUI.onload();
window.CodesearchUI = CodesearchUI;
});
