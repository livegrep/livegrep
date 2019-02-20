$ = require('jquery');

var KeyCodes = {
  ESCAPE: 27,
  ENTER: 13,
  SLASH_OR_QUESTION_MARK: 191
};

function getSelectedText() {
  return window.getSelection ? window.getSelection().toString() : null;
}

function scrollToRange(range, elementContainer) {
  // - If we have a single line, scroll the viewport so that the element is
  // at 1/3 of the viewport.
  // - If we have a range, try and center the range in the viewport
  // - If the range is to high to fit in the viewport, fallback to the single
  //   element scenario for the first line
  var viewport = $(window);
  var viewportHeight = viewport.height();

  var scrollOffset = Math.floor(viewport.height() / 3.0);

  var firstLineElement = elementContainer.find("#L" + range.start);
  if(!firstLineElement.length) {
    // We were given a scroll offset to a line number that doesn't exist in the page, bail
    return;
  }
  if(range.start != range.end) {
    // We have a range, try and center the entire range. If it's to high
    // for the viewport, fallback to revealing the first element.
    var lastLineElement = elementContainer.find("#L" + range.end);
    var rangeHeight = (lastLineElement.offset().top + lastLineElement.height()) - firstLineElement.offset().top;
    if(rangeHeight <= viewportHeight) {
      // Range fits in viewport, center it
      scrollOffset = 0.5 * (viewportHeight - rangeHeight);
    } else {
      scrollOffset = firstLineElement.height() / 2; // Stick to (almost) the top of the viewport
    }
  }

  viewport.scrollTop(firstLineElement.offset().top - scrollOffset);
}

function setHash(hash) {
  if(history.replaceState) {
    history.replaceState(null, null, hash);
  } else {
    location.hash = hash;
  }
}

function parseHashForLineRange(hashString) {
  var parseMatch = hashString.match(/#L(\d+)(?:-L?(\d+))?/);

  if(parseMatch && parseMatch.length === 3) {
    // We have a match on the regex expression
    var startLine = parseInt(parseMatch[1], 10);
    var endLine = parseInt(parseMatch[2], 10);
    if(isNaN(endLine) || endLine < startLine) {
      endLine = startLine;
    }
    return {
      start: startLine,
      end: endLine
    };
  }

  return null;
}

function addHighlightClassesForRange(range, root) {
  var idSelectors = [];
  for(var lineNumber = range.start; lineNumber <= range.end; lineNumber++) {
    idSelectors.push("#L" + lineNumber);
  }
  root.find(idSelectors.join(",")).addClass('highlighted');
}

function expandRangeToElement(element) {
  var range = parseHashForLineRange(document.location.hash);
  if(range) {
    var elementLine = parseInt(element.attr('id').replace('L', ''), 10);
    if(elementLine < range.start) {
      range.end = range.start;
      range.start = elementLine;
    } else {
      range.end = elementLine;
    }
    setHash("#L" + range.start + "-" + range.end);
  }
}

function init(initData) {
  var root = $('.file-content');
  var lineNumberContainer = root.find('.line-numbers');
  var helpScreen = $('.help-screen');

  function doSearch(event, query, newTab) {
    var url;
    if (query !== undefined) {
      url = '/search?q=' + encodeURIComponent(query) + '&repo=' + encodeURIComponent(initData.repo_info.name);
    } else {
      url = '/search';
    }
    if (newTab === true){
      window.open(url);
    } else {
      window.location.href = url
    }
  }

  function showHelp() {
    helpScreen.removeClass('hidden').children().on('click', function(event) {
      // Prevent clicks inside the element to reach the document
      event.stopImmediatePropagation();
      return true;
    });

    $(document).on('click', hideHelp);
  }

  function hideHelp() {
    helpScreen.addClass('hidden').children().off('click');
    $(document).off('click', hideHelp);
    return true;
  }

  function handleHashChange(scrollElementIntoView) {
    if(scrollElementIntoView === undefined) {
      scrollElementIntoView = true; // default if nothing was provided
    }

    // Clear current highlights
    lineNumberContainer.find('.highlighted').removeClass('highlighted');

    // Highlight the current range from the hash, if any
    var range = parseHashForLineRange(document.location.hash);
    if(range) {
      addHighlightClassesForRange(range, lineNumberContainer);
      if(scrollElementIntoView) {
        scrollToRange(range, root);
      }
    }

    // Update the external-browse link
    $('#external-link').attr('href', getExternalLink(range));
    updateFragments(range, $('#permalink, #back-to-head'));
  }

  function getLineNumber(range) {
    if (range == null) {
      // Default to first line if no lines are selected.
      return 1;
    } else if (range.start == range.end) {
      return range.start;
    } else {
      // We blindly assume that the external viewer supports linking to a
      // range of lines. Github doesn't support this, but highlights the
      // first line given, which is close enough.
      return range.start + "-" + range.end;
    }
  }

  function getExternalLink(range) {
    var lno = getLineNumber(range);

    var repoName = initData.repo_info.name;
    var filePath = initData.file_path;

    url = initData.repo_info.metadata['url-pattern']

    // If {path} already has a slash in front of it, trim extra leading
    // slashes from `pathInRepo` to avoid a double-slash in the URL.
    if (url.indexOf('/{path}') !== -1) {
      filePath = filePath.replace(/^\/+/, '');
    }

    // XXX code copied
    url = url.replace('{lno}', lno);
    url = url.replace('{version}', initData.commit);
    url = url.replace('{name}', repoName);
    url = url.replace('{path}', filePath);
    return url;
  }

  function updateFragments(range, $anchors) {
    $anchors.each(function() {
      var $a = $(this);
      var href = $a.attr('href').split('#')[0];
      if (range !== null) {
        href += '#L' + getLineNumber(range);
      }
      $a.attr('href', href);
    });
  }

  function processKeyEvent(event) {
    if(event.which === KeyCodes.ENTER) {
      // Perform a new search with the selected text, if any
      var selectedText = getSelectedText();
      if(selectedText) {
        doSearch(event, selectedText, true);
      }
    } else if(event.which === KeyCodes.SLASH_OR_QUESTION_MARK) {
        event.preventDefault();
        if(event.shiftKey) {
          showHelp();
        } else {
          hideHelp();
          doSearch(event, getSelectedText());
        }
    } else if(event.which === KeyCodes.ESCAPE) {
      // Avoid swallowing the important escape key event unless we're sure we want to
      if(!helpScreen.hasClass('hidden')) {
        event.preventDefault();
        hideHelp();
      }
      $('#query').blur();
    } else if(String.fromCharCode(event.which) == 'V') {
      // Visually highlight the external link to indicate what happened
      $('#external-link').focus();
      window.location = $('#external-link').attr('href');
    } else if (String.fromCharCode(event.which) == 'Y') {
      var $a = $('#permalink');
      var permalink_is_present = $a.length > 0;
      if (permalink_is_present) {
        $a.focus();
        window.location = $a.attr('href');
      }
    } else if (String.fromCharCode(event.which) == 'N' || String.fromCharCode(event.which) == 'P') {
      var goBackwards = String.fromCharCode(event.which) === 'P';
      var selectedText = getSelectedText();
      if (selectedText) {
        window.find(selectedText, false /* case sensitive */, goBackwards);
      }
    }
    return true;
  }

  function initializeActionButtons(root) {
    // Map out action name to function call, and automate the details of actually hooking
    // up the event handling.
    var ACTION_MAP = {
      search: doSearch,
      help: showHelp,
    };

    for(var actionName in ACTION_MAP) {
      root.on('click auxclick', '[data-action-name="' + actionName + '"]',
        // We can't use the action mapped handler directly here since the iterator (`actioName`)
        // will keep changing in the closure of the inline function.
        // Generating a click handler on the fly removes the dependency on closure which
        // makes this work as one would expect. #justjsthings.
        (function(handler) {
          return function(event) {
            event.preventDefault();
            event.stopImmediatePropagation(); // Prevent immediately closing modals etc.
            handler.call(this, event);
          }
        })(ACTION_MAP[actionName])
      )
    }
  }

  var showSelectionReminder = function () {
    $('.without-selection').hide();
    $('.with-selection').show();
  }

  var hideSelectionReminder = function () {
    $('.without-selection').show();
    $('.with-selection').hide();
  }

  function initializePage() {
    // Initial range detection for when the page is loaded
    handleHashChange();

    // Allow shift clicking links to expand the highlight range
    lineNumberContainer.on('click', 'a', function(event) {
      event.preventDefault();
      if(event.shiftKey) {
        expandRangeToElement($(event.target), lineNumberContainer);
      } else {
        setHash($(event.target).attr('href'));
      }
      handleHashChange(false);
    });

    $(window).on('hashchange', function(event) {
      event.preventDefault();
      // The url was updated with a new range
      handleHashChange();
    });

    $(document).on('keydown', function(event) {
      // Filter out key events when the user has focused an input field.
      if($(event.target).is('input,textarea'))
        return;
      // Filter out key if a modifier is pressed.
      if(event.altKey || event.ctrlKey || event.metaKey)
        return;
      processKeyEvent(event);
    });

    $(document).mouseup(function() {
      var selectedText = getSelectedText();
      if(selectedText) {
        showSelectionReminder(selectedText);
      } else {
        hideSelectionReminder();
      }
    });

    initializeActionButtons($('.header .header-actions'));
  }

  // The native browser handling of hashes in the location is to scroll
  // to the element that has a name matching the id. We want to prevent
  // this since we want to take control over scrolling ourselves, and the
  // most reliable way to do this is to hide the elements until the page
  // has loaded. We also need defer our own scroll handling since we can't
  // access the geometry of the DOM elements until they are visible.
  setTimeout(function() {
    lineNumberContainer.css({display: 'block'});
    initializePage();
  }, 1);
}

module.exports = {
  init: init
}
