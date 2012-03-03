"use strict";
var CodesearchUI = function() {
  function shorten(ref) {
    var match = /^refs\/(tags|branches)\/(.*)/.exec(ref);
    if (match)
      return match[2];
    return ref;
  }
  function url_for(match) {
    return "https://github.com/torvalds/linux/blob/" + shorten(match.ref) +
      "/" + match.file + "#L" + match.lno;
  }
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
                   h.div({cls: 'label'},
                         [
                           h.a({
                                 href: url_for(match)
                               }, [match.file])
                         ]),
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
  return {
    displaying: null,
    results: 0,
    search_id: 0,
    onload: function() {
      CodesearchUI.input     = $('#searchbox');
      CodesearchUI.input_file = $('#filebox');
      CodesearchUI.input.keydown(CodesearchUI.keypress);
      CodesearchUI.input_file.keydown(CodesearchUI.keypress);
      Codesearch.connect(CodesearchUI);
    },
    keypress: function() {
      setTimeout(CodesearchUI.newsearch, 0);
    },
    newsearch: function() {
      if (CodesearchUI.input.val().length) {
        Codesearch.new_search(
          CodesearchUI.input.val(),
          CodesearchUI.input_file.val(),
          ++CodesearchUI.search_id);
      }
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
      $('#results').append(render_match(match));
      $('#numresults').text(CodesearchUI.results);
      $('#countarea').show();
    },
    search_done: function(search, time, why) {
      CodesearchUI.handle_result(search);
      if (why === 'limit')
        $('#numresults').append('+');
      $('#countarea').show();
      $('#searchtime').text((time/1000) + "s");
      $('#searchtimebox').show();
    },
    handle_result: function(search) {
      CodesearchUI.hide_error();
      if (search != CodesearchUI.displaying) {
        $('#numresults').text('0');
        $('#results').empty();
        $('#searchtimebox').hide();
        $('#countarea').hide();
        CodesearchUI.displaying = search;
        CodesearchUI.results = 0;
      }
    }
  };
}();
$(document).ready(CodesearchUI.onload);
