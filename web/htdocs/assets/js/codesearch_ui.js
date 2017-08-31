$(function() {
"use strict";

var h = new HTMLFactory();

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
  // If reference is origin/foo, assume that foo is
  // the branch name.
  match = /^origin\/(.*)/.exec(ref);
  if (match) {
    return match[1];
  }
  return ref;
}

function url(tree, version, path, lno) {
  if (tree in CodesearchUI.intervalViewRepos) {
    return internalUrl(tree, path, lno);
  } else {
    return externalUrl(tree, version, path, lno);
  }
}

function internalUrl(tree, path, lno) {
  var url = "/view/" + tree + "/" + path;
  if (lno !== undefined) {
    url += "#L" + lno;
  }
  return url;
}

function externalUrl(tree, version, path, lno) {
  var backend = Codesearch.in_flight.backend;
  var repo_map = CodesearchUI.repo_urls[backend];
  if (!repo_map) {
    return null;
  }
  if (!repo_map[tree]) {
    return null;
  }

  if (lno === undefined) {
      lno = 1;
  }

  // the order of these replacements is used to minimize conflicts
  var url = repo_map[tree];
  url = url.replace('{lno}', lno);
  url = url.replace('{version}', shorten(version));
  url = url.replace('{name}', tree);
  url = url.replace('{path}', path);
  return url;
}

var MatchView = Backbone.View.extend({
  tagName: 'div',
  initialize: function() {
    this.model.on('change', this.render, this);
  },
  render: function() {
    var div = this._render();
    this.setElement(div);
    return this;
  },
  _renderLno: function(n, isMatch) {
    var lnoStr = n.toString() + (isMatch ? ":" : "-");
    var classes = ['lno-link'];
    if (isMatch) classes.push('matchlno');
    return h.a({cls: classes.join(' '), href: this.model.url(n)}, [
      h.span({cls: 'lno', 'aria-label': lnoStr}, [])
    ]);
  },
  _render: function() {
    var i;
    var ctx_before = [], ctx_after = [];
    var lno = this.model.get('lno');
    var ctxBefore = this.model.get('context_before'), clip_before = this.model.get('clip_before');
    var ctxAfter = this.model.get('context_after'), clip_after = this.model.get('clip_after');

    var lines_to_display_before = Math.max(0, ctxBefore.length - (clip_before || 0));
    for (i = 0; i < lines_to_display_before; i ++) {
      ctx_before.unshift(
        this._renderLno(lno - i - 1, false),
        h.span([this.model.get('context_before')[i]])
      );
    }
    var lines_to_display_after = Math.max(0, ctxAfter.length - (clip_after || 0));
    for (i = 0; i < lines_to_display_after; i ++) {
      ctx_after.push(
        this._renderLno(lno + i + 1, false),
        h.span([this.model.get('context_after')[i]])
      );
    }
    var line = this.model.get('line');
    var bounds = this.model.get('bounds');
    var pieces = [line.substring(0, bounds[0]),
                  line.substring(bounds[0], bounds[1]),
                  line.substring(bounds[1])];

    var classes = ['match'];
    if(clip_before !== undefined) classes.push('clip-before');
    if(clip_after !== undefined) classes.push('clip-after');

    var matchElement = h.div({cls: classes.join(' ')}, [
      h.div({cls: 'contents'}, [].concat(
        ctx_before,
        [
            this._renderLno(lno, true),
            h.span({cls: 'matchline'}, [pieces[0], h.span({cls: 'matchstr'}, [pieces[1]]), pieces[2]])
        ],
        ctx_after
      ))
    ]);

    return matchElement;
  }
});

/**
 * A Match represents a single match in the code base.
 *
 * This model wraps the JSON response from the Codesearch backend for an individual match.
 */
var Match = Backbone.Model.extend({
  path_info: function() {
    var tree = this.get('tree');
    var version = this.get('version');
    var path = this.get('path');
    return {
      id: tree + ':' + version + ':' + path,
      tree: tree,
      version: version,
      path: path
    }
  },

  url: function(lno) {
    if (lno === undefined) {
      lno = this.get('lno');
    }
    return url(this.get('tree'), this.get('version'), this.get('path'), lno);
  },
});

/** A set of Matches at a single path. */
var FileGroup = Backbone.Model.extend({
  initialize: function(path_info) {
    // The id attribute is used by collections to fetch models
    this.id = path_info.id;
    this.path_info = path_info;
    this.matches = [];
  },

  add_match: function(match) {
    this.matches.push(match);
  },

  /** Prepare the matches for rendering by clipping the context of matches to avoid duplicate
   *  lines being displayed in the search results.
   *
   * This function operates under these assumptions:
   * - The matches are all for the same file
   * - Two matches cannot have the same line number
   */
  process_context_overlaps: function() {
    if(!(this.matches) || this.matches.length < 2) {
      return; // We don't have overlaps unless we have at least two things
    }

    // NOTE: The logic below requires matches to be sorted by line number.
    this.matches.sort(function(a, b) {
      return a.get('lno') - b.get('lno');
    });

    for(var i = 1, len = this.matches.length; i < len; i++) {
      var previous_match = this.matches[i - 1], this_match = this.matches[i];
      var last_line_of_prev_context = previous_match.get('lno') + previous_match.get('context_after').length;
      var first_line_of_this_context = this_match.get('lno') - this_match.get('context_before').length;
      var num_intersecting_lines = (last_line_of_prev_context - first_line_of_this_context) + 1;
      if(num_intersecting_lines >= 0) {
        // The matches are intersecting or share a boundary.
        // Try to split the context between the previous match and this one.
        // Uneven splits should leave the latter element with the larger piece.

        // split_at will be the first line number grouped with the latter element.
        var split_at = parseInt(Math.ceil((previous_match.get('lno') + this_match.get('lno')) / 2.0), 10);
        if (split_at < first_line_of_this_context) {
            split_at = first_line_of_this_context;
        } else if (last_line_of_prev_context + 1 < split_at) {
            split_at = last_line_of_prev_context + 1;
        }

        var clip_for_previous = last_line_of_prev_context - (split_at - 1);
        var clip_for_this = split_at - first_line_of_this_context;
        previous_match.set('clip_after', clip_for_previous);
        this_match.set('clip_before', clip_for_this);
      } else {
        previous_match.unset('clip_after');
        this_match.unset('clip_before');
      }
    }
  }
});

/** A set of matches that are automatically grouped by path. */
var SearchResultSet = Backbone.Collection.extend({
  comparator: function(file_group) {
    return file_group.id;
  },

  add_match: function(match) {
    var path_info = match.path_info();
    var file_group = this.get(path_info.id);
    if(!file_group) {
      file_group = new FileGroup(path_info);
      this.add(file_group);
    }
    file_group.add_match(match);
  },

  num_matches: function() {
    return this.reduce(function(memo, file_group) {
      return memo + file_group.matches.length;
    }, 0);
  }
});

/**
 * A FileMatch represents a single filename match in the code base.
 *
 * This model wraps the JSON response from the Codesearch backend for an individual match.
 *
 * XXX almost identical to Match
 */
var FileMatch = Backbone.Model.extend({
  path_info: function() {
    var tree = this.get('tree');
    var version = this.get('version');
    var path = this.get('path');
    return {
      id: tree + ':' + version + ':' + path,
      tree: tree,
      version: version,
      path: path,
      bounds: this.get('bounds')
    }
  },

  url: function() {
    return url(this.get('tree'), this.get('version'), this.get('path'));
  },
});

var FileMatchView = Backbone.View.extend({
  tagName: 'div',

  render: function() {
    var path_info = this.model.path_info();
    var pieces = [
      path_info.path.substring(0, path_info.bounds[0]),
      path_info.path.substring(path_info.bounds[0], path_info.bounds[1]),
      path_info.path.substring(path_info.bounds[1])
    ];
    var repoLabel = [
      h.span({cls: "repo"}, [path_info.tree, ':']),
      h.span({cls: "version"}, [shorten(path_info.version), ':']),
      pieces[0],
      h.span({cls: "matchstr"}, [pieces[1]]),
      pieces[2]
    ];

    var el = this.$el;
    el.empty();
    el.addClass('filename-match');
    el.append(h.a({cls: 'label header result-path', href: this.model.url()}, repoLabel));
    return this;
  }
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
    this.search_results = new SearchResultSet();
    this.file_search_results = new Backbone.Collection();
    this.search_id = 0;
    this.on('change:displaying', this.new_search, this);
  },

  new_search: function() {
    this.set({
        error: null,
        time: null,
        why: null
    });
    this.search_results.reset();
    this.file_search_results.reset();
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
        cur.q === search.q &&
        cur.fold_case === search.fold_case &&
        cur.regex === search.regex &&
        cur.backend === search.backend) {
      return false;
    }
    var id = this.next_id();
    search.id = id;
    this.search_map[id] = {
      q: search.q,
      fold_case: search.fold_case,
      regex: search.regex,
      backend: search.backend
    };
    if (!search.q.length) {
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

    if (current.q !== "") {
      q.q = current.q;
      q.fold_case = current.fold_case;
      q.regex = current.regex;
    }

    if (current.backend) {
      base += "/" + current.backend
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
    this.search_results.add_match(new Match(m));
  },
  handle_file_match: function (search, file_match) {
    if (search < this.get('displaying'))
      return false;
    this.set('displaying', search);
    var fm = _.clone(file_match);
    fm.backend = this.search_map[search].backend;
    // TODO: Currently we hackily limit the display to 10 file-path results.
    // We should do something nicer, like a "..." the user can click to extend
    // the list.
    if (this.file_search_results.length < 10) {
        this.file_search_results.add(new FileMatch(fm));
    }
  },
  handle_done: function (search, time, why) {
    if (search < this.get('displaying'))
      return false;
    this.set('displaying', search);
    this.set({time: time, why: why});
    this.search_results.trigger('search-complete');
  }
});

var FileGroupView = Backbone.View.extend({
  tagName: 'div',

  render_header: function(tree, version, path) {
    var basename, dirname;
    var indexOfLastPathSep = path.lastIndexOf('/');

    if(indexOfLastPathSep !== -1) {
      basename = path.substring(indexOfLastPathSep + 1, path.length);
      dirname = path.substring(0, indexOfLastPathSep + 1);
    } else {
      basename = path; // path doesn't contain any dir parts, only the basename
      dirname = '';
    }

    var repoLabel = [
      h.span({cls: "repo"}, [tree, ':']),
      h.span({cls: "version"}, [shorten(version), ':']),
      dirname,
      h.span({cls: "filename"}, [basename])
    ];
    return h.a({cls: 'label header result-path', href: this.model.matches[0].url()}, repoLabel);
  },

  render: function() {
    var matches = this.model.matches;
    var el = this.$el;
    el.empty();
    el.append(this.render_header(this.model.path_info.tree, this.model.path_info.version, this.model.path_info.path));
    matches.forEach(function(match) {
      el.append(
        new MatchView({model:match}).render().el
      );
    });
    el.addClass('file-group');
    return this;
  }
});

var MatchesView = Backbone.View.extend({
  el: $('#results'),
  initialize: function() {
    this.model.search_results.on('search-complete', this.render, this);
    this.model.search_results.on('rerender', this.render, this);
  },
  render: function() {
    this.$el.empty();

    var pathResults = h.div({'cls': 'path-results'});
    this.model.file_search_results.each(function(file) {
        var view = new FileMatchView({model: file});
        pathResults.append(view.render().el);
    }, this);
    this.$el.append(pathResults);

    this.model.search_results.each(function(file_group) {
      file_group.process_context_overlaps();
      var view = new FileGroupView({model: file_group});
      this.$el.append(view.render().el);
    }, this);
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
    this.model.search_results.on('all', this.render, this);
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
      $('#helparea').show();
      return this;
    }

    this.$el.show();
    $('#helparea').hide();

    if (this.model.get('time')) {
      this.$('#searchtimebox').show();
      var time = this.model.get('time');
      this.time.text((time/1000) + "s")
    } else {
      this.$('#searchtimebox').hide();
    }

    var results = '' + this.model.search_results.num_matches();
    if (this.model.get('why') === 'MATCH_LIMIT')
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
      CodesearchUI.input_backend = $('#backend');
      if (CodesearchUI.input_backend.length == 0)
        CodesearchUI.input_backend = null;
      CodesearchUI.inputs_case = $('input[name=fold_case]');
      CodesearchUI.input_regex = $('input[name=regex]');

      if (CodesearchUI.inputs_case.filter(':checked').length == 0) {
          CodesearchUI.inputs_case.filter('[value=auto]').attr('checked', true);
      }

      CodesearchUI.parse_url();

      CodesearchUI.input.keydown(CodesearchUI.keypress);
      CodesearchUI.input.bind('paste', CodesearchUI.keypress);
      CodesearchUI.input.focus();
      if (CodesearchUI.input_backend)
        CodesearchUI.input_backend.change(CodesearchUI.select_backend);

      CodesearchUI.inputs_case.change(CodesearchUI.keypress);
      CodesearchUI.input_regex.change(CodesearchUI.keypress);

      CodesearchUI.input_context = $('input[name=context]');
      CodesearchUI.input_context.change(function(){
        $('#results').toggleClass('no-context', !CodesearchUI.input_context.attr('checked'));
      });

      Codesearch.connect(CodesearchUI);
    },
    parse_url: function() {
      var parms = CodesearchUI.parse_query_params();

      var q = [];
      if (parms.q)
        q.push(parms.q);
      if (parms.file)
        q.push("file:" + parms.file);
      if (parms.repo)
        q.push("repo:" + parms.repo);
      CodesearchUI.input.val(q.join(' '));

      if (parms.fold_case) {
        CodesearchUI.inputs_case.filter('[value='+parms.fold_case+']').attr('checked', true);
      }
      if (parms.regex === "true") {
        CodesearchUI.input_regex.prop('checked', true);
      }

      var backend = null;
      if (parms.backend)
        backend = parms.backend;
      var m;
      if (m = (new RegExp("/search/([^\/]+)/?").exec(window.location.pathname))) {
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
      CodesearchUI.keypress();
    },
    keypress: function() {
      CodesearchUI.clear_timer();
      CodesearchUI.timer = setTimeout(CodesearchUI.newsearch, 100);
    },
    newsearch: function() {
      CodesearchUI.clear_timer();
      var search = {
        q: CodesearchUI.input.val(),
        fold_case: CodesearchUI.inputs_case.filter(':checked').val(),
        regex: CodesearchUI.input_regex.is(':checked'),
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
    file_match: function(search, file_match) {
      CodesearchUI.state.handle_file_match(search, file_match);
    },
    search_done: function(search, time, why) {
      CodesearchUI.state.handle_done(search, time, why);
    },
    repo_urls: {}
  };
}();
window.CodesearchUI = CodesearchUI;
});
