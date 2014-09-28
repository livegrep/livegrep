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
  },
  url: function() {
    var name = this.get('tree');
    var ref = this.get('version');

    var repo_map;
    if (this.get('backend'))
      repo_map = CodesearchUI.github_repos[this.get('backend')]
    else
      repo_map = CodesearchUI.github_repos[Object.keys(CodesearchUI.github_repos)[0]]
    if (!repo_map[name])
      return null;
    return "https://github.com/" + repo_map[name] +
      "/blob/" + shorten(ref) + "/" + this.get('path') +
      "#L" + this.get('lno');
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
    var i;
    var ctx_before = [], ctx_after = [];
    var lno = this.model.get('lno');
    for (i = 0; i < this.model.get('context_before').length; i ++) {
      ctx_before.unshift(h.div([
                                 h.span({cls: 'lno'}, [lno - i - 1, ":"]),
                                 this.model.get('context_before')[i]
                               ]));
    }
    for (i = 0; i < this.model.get('context_after').length; i ++) {
      ctx_after.push(h.div([
                             h.span({cls: 'lno'}, [lno + i + 1, ":"]),
                             this.model.get('context_after')[i]
                           ]));
    }
    var line = this.model.get('line');
    var bounds = this.model.get('bounds');
    var pieces = [line.substring(0, bounds[0]),
                  line.substring(bounds[0], bounds[1]),
                  line.substring(bounds[1])];

    var tree = this.model.get('tree');
    var version = this.model.get('version');
    var repoLabel = [
      tree ? (tree + ":") : "",
      shorten(version),
      ":",
      this.model.get('path')];
    var url = this.model.url();
    if (url !== null) {
      repoLabel = [ h.a({href: this.model.url()}, repoLabel) ];
    }
    return h.div({cls: 'match'}, [
        h.div({}, [
          h.span({cls: 'label'}, repoLabel)]),
        h.div({cls: 'contents'}, [
                ctx_before,
                h.div({cls: 'matchline'}, [
                  h.span({cls: 'lno'}, [lno + ":"]),
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
      if (parseInt(k) < this.get('displaying'))
        delete this.search_map[k];
    }
  },

  next_id: function() {
    return ++this.search_id;
  },

  dispatch: function (search) {
    var cur = this.search_map[this.get('displaying')];
    if (cur &&
        cur.q === search.line &&
        cur.file === search.file &&
        cur.backend === search.backend &&
        cur.repo === search.repo &&
        cur.fold_case === search.fold_case) {
      return false;
    }
    var id = this.next_id();
    search.id = id;
    this.search_map[id] = {
      q: search.line,
      file: search.file,
      backend: search.backend,
      repo: search.repo,
      fold_case: search.fold_case
    };
    if (!search.line.length) {
      this.set('displaying', id);
      return false;
    }
    return true;
  },

  url: function() {
    var q = {};
    var current = this.search_map[this.get('displaying')];
    if (!current)
      return '/search';
    var base = '/search';

    ['q','file', 'repo', 'fold_case'].forEach(function (key) {
      if(current[key])
        q[key] = current[key];
    });

    if (q.backend) {
      base += "/" + q.backend
    } else if (CodesearchUI.input_backend) {
      base += "/" + CodesearchUI.input_backend.val();
    }
    var qs = $.param(q);
    return base + (qs ? "?" + qs : "");
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
    var m = _.clone(match);
    m.backend = this.search_map[search].backend;
    this.matches.add(m);
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
      CodesearchUI.input_repo = $('#repobox');
      CodesearchUI.input_fold_case = $('#fold-case');
      CodesearchUI.input_backend = $('#backend');
      if (CodesearchUI.input_backend.length == 0)
        CodesearchUI.input_backend = null;
      CodesearchUI.parse_url();

      CodesearchUI.input.keydown(CodesearchUI.keypress);
      CodesearchUI.input_file.keydown(CodesearchUI.keypress);
      CodesearchUI.input_repo.keydown(CodesearchUI.keypress);
      CodesearchUI.input.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input_file.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input_repo.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input_fold_case.bind('click', CodesearchUI.keypress);
      CodesearchUI.input.focus();
      if (CodesearchUI.input_backend)
        CodesearchUI.input_backend.change(CodesearchUI.select_backend);

      Codesearch.connect(CodesearchUI);
    },
    parse_url: function() {
      var parms = CodesearchUI.parse_query_params();
      if (parms.q)
        CodesearchUI.input.val(parms.q);
      if (parms.file)
        CodesearchUI.input_file.val(parms.file);
      if (parms.repo)
        CodesearchUI.input_repo.val(parms.repo);
      if (parms.fold_case === 'true')
        CodesearchUI.input_fold_case.attr('checked', true);
      var backend = null;
      if (parms.backend)
        backend = parms.backend;
      var m;
      if (m = (new RegExp("/search/(\\w+)/?").exec(window.location.pathname))) {
        backend = m[1];
      }
      if (backend && CodesearchUI.input_backend)
        CodesearchUI.input_backend.val(backend);
      setTimeout(CodesearchUI.select_backend, 0);
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
    select_backend: function() {
      if (!CodesearchUI.input_backend)
        return;
      var backend = CodesearchUI.input_backend.val();
      if (Object.keys(CodesearchUI.github_repos[backend]).length == 1) {
        CodesearchUI.input_repo.val('');
        $('#reposel').hide();
      } else {
        $('#reposel').show();
      }
      CodesearchUI.keypress();
    },
    keypress: function() {
      CodesearchUI.clear_timer();
      CodesearchUI.timer = setTimeout(CodesearchUI.newsearch, 100);
    },
    newsearch: function() {
      CodesearchUI.clear_timer();
      var search = {
        line: CodesearchUI.input.val(),
        file: CodesearchUI.input_file.val(),
        repo: CodesearchUI.input_repo.val(),
        fold_case: !!CodesearchUI.input_fold_case.attr('checked')
      };
      if (CodesearchUI.input_backend)
        search.backend = CodesearchUI.input_backend.val();
      if (CodesearchUI.state.dispatch(search))
        Codesearch.new_search(search);
    },
    clear_timer: function() {
      if (CodesearchUI.timer) {
        clearTimeout(CodesearchUI.timer);
        CodesearchUI.timer = null;
      }
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
    github_repos: {}
  };
}();
CodesearchUI.onload();
window.CodesearchUI = CodesearchUI;
});
