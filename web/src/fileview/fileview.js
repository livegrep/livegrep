$ = require('jquery');

var KeyCodes = {
  ESCAPE: 27,
  ENTER: 13,
  SLASH_OR_QUESTION_MARK: 191
};

function getSelectedText() {
  return window.getSelection ? window.getSelection().toString() : null;
}

function doSearch(event, query, newTab, initData) {
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
  var parseMatch = hashString.match(/#L(\d+)(?:-(\d+))?/);

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
    } else if (range.start === range.end) {
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

    url = initData.repo_info.metadata['url-pattern'];

    // XXX code copied
    url = url.replace('{lno}', lno);
    url = url.replace('{version}', initData.commit);
    url = url.replace('{name}', initData.repo_info.name);
    url = url.replace('{path}', initData.repo_info.path);
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

  // compute the offset from the start of the parent containing the click
  function textBeforeOffset(childNode, childOffset, parentNode) {
    // create a new range starting at the beginning of the parent and going until the selection
    const rangeBeforeClick = new Range();
    rangeBeforeClick.setStart(parentNode, 0);
    rangeBeforeClick.setEnd(childNode, childOffset);
    return rangeBeforeClick.toString();
  }

  // returns range for symbol containing the specified location.
  // symbol is determined by greedily absorbing alphanumerics and underscores.
  function symbolAtLocation(textNode, offset) {
    const stringBefore = textNode.nodeValue.substring(0, offset);
    const stringAfter = textNode.nodeValue.substring(offset);
    const startIndex = stringBefore.match(/[a-zA-Z0-9_]*$/).index;
    const endIndex = stringBefore.length + stringAfter.match(/^[a-zA-Z0-9_]*/)[0].length;
    const range = new Range();
    range.setStart(textNode, startIndex);
    range.setEnd(textNode, endIndex);
    return range;
  }

  function triggerJumpToDef() {
      const nodeClicked = document.getSelection().anchorNode.parentNode;
      const cachedUrl = nodeClicked.getAttribute('definition-url');
      if (cachedUrl) {
        window.location.href = cachedUrl;
      }
  }

  var hoveringNode = null;
  var mousePositionX = 0;
  var mousePositionY = 0;

  function cancelHover() {
    if (hoveringNode) {
      hoveringNode.className = 'hoverable';
    }
    hoveringNode = null;
  }

  function hoverOverNode(node) {
    node.className = 'hovering';
    hoveringNode = node;
  }

  function checkIfHoverable(node) {
    const code = document.getElementById('source-code');
    const stringBefore = textBeforeOffset(node, 0, code);

    const rows = stringBefore.split('\n');
    // The server expects the rows to be zero-indexed.
    const row = rows.length - 1;
    const col = rows[row].length;

    xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.status === 200 && this.responseText) {
        const resp = JSON.parse(this.responseText);
        node.setAttribute('definition-url', resp.url);
        if (isInBox(mousePositionX, mousePositionY, node.getBoundingClientRect())) {
          hoverOverNode(node);
        } else {
          // The mouse has moved and is not be on this element.
          node.className = 'hoverable';
        }
      } else {
        node.className = 'nonhoverable';
      }
    };

    const url = '/api/v1/langserver/jumptodef?repo_name='
        + initData.repo_info.name
        + "&file_path="
        + initData.file_path
        + "&row=" + row + "&col=" + col;
    xhttp.open("GET", url);
    xhttp.send();
  }

  function isInBox(x, y, rect) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
  }

  // When source code is hovered over, highlight/underline any tokens for which
  // jump-to-definition will work.
  function onHover(clientX, clientY) {
    // The source-code consists of a <code id='source-code' class='code-pane'>
    // containing lots of <span class="token tokentype">token</span>
    // hoverable text may be surrounded by <span class="hoverable">
    // text being hovered over is changed to class="hovering"
    // non-hoverable text is class="nonhoverable"
    const pos = document.caretRangeFromPoint(clientX, clientY);
    const textNode = pos.startContainer;
    if (textNode.nodeType !== 3) { // expected to be a text node
      return;
    }
    // decide what to do based on the class of the span containing the text
    const node = textNode.parentNode;
    const nodeClass = node.className;
    if (nodeClass === 'hovering') {
      return;
    }
    cancelHover();
    if (!nodeClass || nodeClass === 'nonhoverable') {
      return;
    }
    if (nodeClass === 'hoverable') {
      hoverOverNode(node);
      return;
    }
    const tokenType = nodeClass.match(/token ([a-z]+)/);
    if (tokenType) {
      // the only token which potentially has a definition is a 'token function'
      if (tokenType[1] === 'function') {
        node.innerHTML = "<span>" + node.innerHTML + "</span>";
        checkIfHoverable(node.childNodes[0]);
      }
      return;
    }
    if (node.id !== 'source-code') {
      return;
    }
    // syntax highlighter hasn't identified the token yet, so we have to parse
    // to find the token ourselves, and create a new span around it.
    const symbolRange = symbolAtLocation(textNode, pos.startOffset);
    if (symbolRange.toString().length === 0) {
      return;
    }
    const newSpan = document.createElement('span');
    symbolRange.surroundContents(newSpan);
    checkIfHoverable(newSpan);
  }

  function processKeyEvent(event) {
    if(event.which === KeyCodes.ENTER) {
      // Perform a new search with the selected text, if any
      var selectedText = getSelectedText();
      if(selectedText) {
        doSearch(event, selectedText, true, initData);
      }
    } else if(event.which === KeyCodes.SLASH_OR_QUESTION_MARK) {
        event.preventDefault();
        if(event.shiftKey) {
          showHelp();
        } else {
          hideHelp();
          doSearch(event, getSelectedText(), false, initData);
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

    $(document).on('click', function (event) {
      if (initData.has_lang_server) {
        triggerJumpToDef(event);
      }
    });

    $('#source-code').on('mousemove', function (event) {
      if (initData.has_lang_server) {
        mousePositionX = event.clientX;
        mousePositionY = event.clientY;
        onHover(event.clientX, event.clientY);
      }
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
