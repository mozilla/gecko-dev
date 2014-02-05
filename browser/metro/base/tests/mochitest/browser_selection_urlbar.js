/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;

const kCommonWaitMs = 5000;
const kCommonPollMs = 100;

///////////////////////////////////////////////////
// text area tests
///////////////////////////////////////////////////

gTests.push({
  desc: "normalize browser",
  run: function test() {
    info(chromeRoot + "res/textblock01.html");
    yield addTab(chromeRoot + "res/textblock01.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      });

    yield hideContextUI();

    InputSourceHelper.isPrecise = false;
    InputSourceHelper.fireUpdate();
  },
});

gTests.push({
  desc: "nav bar display",
  run: function test() {
    gWindow = window;

    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");

    sendElementTap(window, edit, 100, 10);

    ok(SelectionHelperUI.isSelectionUIVisible, "selection ui active");

    sendElementTap(window, edit, 70, 10);

    ok(SelectionHelperUI.isCaretUIVisible, "caret ui active");

    // to the right
    let xpos = SelectionHelperUI.caretMark.xPos;
    let ypos = SelectionHelperUI.caretMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 900, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(edit).toString() ==
        "mochitests/content/metro/browser/metro/base/tests/mochitest/res/textblock01.html";
    }, kCommonWaitMs, kCommonPollMs);
    touchdrag.end();
    yield waitForMs(100);

    ok(SelectionHelperUI.isSelectionUIVisible, "selection ui active");
  },
});

gTests.push({
  desc: "bug 887120 - tap & hold to paste into urlbar",
  run: function() {
    gWindow = window;

    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");

    SpecialPowers.clipboardCopyString("mozilla");
    sendContextMenuClickToElement(window, edit);
    yield waitForEvent(document, "popupshown");

    ok(ContextMenuUI._menuPopup.visible, "is visible");
    let paste = document.getElementById("context-paste");
    ok(!paste.hidden, "paste item is visible");

    sendElementTap(window, paste);
    ok(edit.popup.popupOpen, "bug: popup should be showing");

    clearSelection(edit);
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

gTests.push({
  desc: "bug 895284 - tap selection",
  run: function() {
    gWindow = window;

    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");
    edit.value = "wikipedia.org";
    edit.select();

    let editCoords = logicalCoordsForElement(edit);

    // wait for popup animation to complete, it interferes with edit selection testing
    let autocompletePopup = document.getElementById("urlbar-autocomplete-scroll");
    yield waitForEvent(autocompletePopup, "transitionend");

    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x, editCoords.y);
    ok(SelectionHelperUI.isSelectionUIVisible, "selection enabled");

    let selection = edit.QueryInterface(Components.interfaces.nsIDOMXULTextBoxElement)
                        .editor.selection;
    let rects = selection.getRangeAt(0).getClientRects();
    let midX = Math.ceil(((rects[0].right - rects[0].left) * .5) + rects[0].left);
    let midY = Math.ceil(((rects[0].bottom - rects[0].top) * .5) + rects[0].top);

    sendTap(window, midX, midY);

    ok(SelectionHelperUI.isCaretUIVisible, "caret browsing enabled");

    clearSelection(edit);
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

gTests.push({
  desc: "bug 894713 - blur shuts down selection handling",
  run: function() {
    gWindow = window;
    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");
    edit.value = "wikipedia.org";
    edit.select();
    let editCoords = logicalCoordsForElement(edit);
    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x, editCoords.y);
    edit.blur();
    ok(!SelectionHelperUI.isSelectionUIVisible, "selection no longer enabled");
    clearSelection(edit);
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

function getClipboardCondition(aExpected) {
  return () => aExpected == SpecialPowers.getClipboardData("text/unicode");
}

gTests.push({
  desc: "bug 894715 - URLs selected by touch are copied with trimming",
  run: function () {
    gWindow = window;
    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");
    edit.value = "http://www.wikipedia.org/";

    sendElementTap(window, edit);
    edit.select();

    let panel = ContextMenuUI._menuPopup._panel;
    let promise = waitForEvent(panel, "popupshown")
    sendContextMenuClickToElement(window, edit);
    ok((yield promise), "show context menu");

    let copy = document.getElementById("context-copy");
    ok(!copy.hidden, "copy menu item is visible")

    let condition = getClipboardCondition("http://www.wikipedia.org/");
    let promise = waitForCondition(condition);
    sendElementTap(window, copy);
    ok((yield promise), "copy text onto clipboard")

    clearSelection(edit);
    edit.blur();
  }
})

gTests.push({
  desc: "bug 965832 - selection monocles move with the nav bar",
  run: function() {
    yield showNavBar();

    let originalUtils = Services.metro;
    Services.metro = {
      keyboardHeight: 0,
      keyboardVisible: false
    };
    registerCleanupFunction(function() {
      Services.metro = originalUtils;
    });

    let edit = document.getElementById("urlbar-edit");
    edit.value = "http://www.wikipedia.org/";

    sendElementTap(window, edit);
    
    let promise = waitForEvent(window, "MozDeckOffsetChanged");
    Services.metro.keyboardHeight = 300;
    Services.metro.keyboardVisible = true;
    Services.obs.notifyObservers(null, "metro_softkeyboard_shown", null);
    yield promise;

    yield waitForCondition(function () {
      return SelectionHelperUI.isSelectionUIVisible;
    });

    promise = waitForEvent(window, "MozDeckOffsetChanged");
    Services.metro.keyboardHeight = 0;
    Services.metro.keyboardVisible = false;
    Services.obs.notifyObservers(null, "metro_softkeyboard_hidden", null);
    yield promise;

    yield waitForCondition(function () {
      return SelectionHelperUI.isSelectionUIVisible;
    });

    clearSelection(edit);
    edit.blur();

    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  runTests();
}
