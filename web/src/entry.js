$ = require('jquery');

pages = {
  codesearch: require('codesearch/codesearch_ui.js'),
  fileview: require('fileview/fileview.js')
};

$(function(){
  if (window.page) {
    pages[window.page].init(window.scriptData);
  }
});
