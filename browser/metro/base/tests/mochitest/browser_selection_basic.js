/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;

const kMarkerOffsetY = 6;
const kCommonWaitMs = 5000;
const kCommonPollMs = 100;

///////////////////////////////////////////////////
// content (non-editable) tests
///////////////////////////////////////////////////

function setUpAndTearDown() {
  emptyClipboard();
  if (gWindow)
    clearSelection(gWindow);
  if (gFrame)
    clearSelection(gFrame);
  yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    }, kCommonWaitMs, kCommonPollMs);
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_basic.html");
    yield addTab(chromeRoot + "browser_selection_basic.html");

    yield waitForCondition(function () {
        return !BrowserUI.isStartTabVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
  },
});

gTests.push({
  desc: "tap-hold to select",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gWindow).toString(), "There", "selection test");
  },
});

gTests.push({
  desc: "appbar interactions",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing", "selection test");

    yield fireAppBarDisplayEvent();

    ok(ContextUI.isVisible, true, "appbar visible");

    yield hideContextUI();

    ok(!ContextUI.isVisible, true, "appbar hidden");
  },
});

gTests.push({
  desc: "simple drag selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    yield waitForMs(100);
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing", "selection test");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 190, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing so VERY", "selection test");
  },
});

gTests.push({
  desc: "expand / collapse selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "initial active");
    is(getTrimmedSelection(gWindow).toString(), "There", "initial selection test");

    for (let count = 0; count < 5; count++) {
      let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

      let touchdrag = new TouchDragAndHold();
      yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
      touchdrag.end();

      yield waitForCondition(function () {
          return !SelectionHelperUI.hasActiveDrag;
        }, kCommonWaitMs, kCommonPollMs);
      yield SelectionHelperUI.pingSelectionHandler();

      is(getTrimmedSelection(gWindow).toString(),
        "There was nothing so VERY remarkable in that; nor did Alice think it so",
        "long selection test");

      is(SelectionHelperUI.isActive, true, "selection active");

      touchdrag = new TouchDragAndHold();
      yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 40, ypos);
      touchdrag.end();

      yield waitForCondition(function () {
          return !SelectionHelperUI.hasActiveDrag;
        }, kCommonWaitMs, kCommonPollMs);
      yield SelectionHelperUI.pingSelectionHandler();

      is(SelectionHelperUI.isActive, true, "selection active");
      is(getTrimmedSelection(gWindow).toString(), "There was", "short selection test");
    }
  },
});

gTests.push({
  desc: "expand / collapse selection scolled content",
  setUp: setUpAndTearDown,
  run: function test() {
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, 200);
    yield scrollPromise;
    ok(scrollPromise && !(scrollPromise instanceof Error), "scrollPromise error");

    sendContextMenuClick(106, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "moment", "selection test");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went Alice after it, never once considering how in",
      "selection test");

    is(SelectionHelperUI.isActive, true, "selection active");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 150, ypos);
    touchdrag.end();
    
    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(), "moment down went", "selection test");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went Alice after it, never once considering how in",
      "selection test");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 160, ypos);
    touchdrag.end();
    
    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went",
      "selection test");
  },
  tearDown: function tearDown() {
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, -200);
    yield scrollPromise;
    emptyClipboard();
    if (gWindow)
      clearSelection(gWindow);
    if (gFrame)
      clearSelection(gFrame);
    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
    yield hideContextUI();
  },
});

gTests.push({
  desc: "tap on selection clears selection in content",
  setUp: setUpAndTearDown,
  run: function test() {

    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    sendTap(gWindow, 30, 20);

    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
  },
  tearDown: setUpAndTearDown,
});

gTests.push({
  desc: "tap off selection clears selection in content",
  setUp: setUpAndTearDown,
  run: function test() {

    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    sendTap(gWindow, 30, 100);

    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
  },
  tearDown: setUpAndTearDown,
});

gTests.push({
  desc: "bug 903737 - right click targeting",
  setUp: setUpAndTearDown,
  run: function test() {
    yield hideContextUI();
    let range = gWindow.document.createRange();
    range.selectNode(gWindow.document.getElementById("seldiv"));
    gWindow.getSelection().addRange(range);
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(gWindow, gWindow.document.getElementById("seldiv"));
    yield promise;
    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    let emptydiv = gWindow.document.getElementById("emptydiv");
    let coords = logicalCoordsForElement(emptydiv);
    InputSourceHelper.isPrecise = true;
    sendContextMenuClick(coords.x, coords.y);
    yield waitForCondition(function () {
      return ContextUI.tabbarVisible;
    });
    yield hideContextUI();
  },
  tearDown: setUpAndTearDown,
});

gTests.push({
  desc: "Bug 960886 - selection monocles being spilled over to other tabs " +
        "when switching.",
  setUp: setUpAndTearDown,
  run: function test() {
    let initialTab = Browser.selectedTab;

    // Create additional tab to which we will switch later
    info(chromeRoot + "browser_selection_basic.html");
    let lastTab = yield addTab(chromeRoot + "browser_selection_basic.html");

    // Switch back to the initial tab
    let tabSelectPromise = waitForEvent(Elements.tabList, "TabSelect");
    Browser.selectedTab = initialTab;
    yield tabSelectPromise;
    yield hideContextUI();

    // Make selection
    sendContextMenuClick(30, 20);
    yield waitForCondition(()=>SelectionHelperUI.isSelectionUIVisible);

    // Switch to another tab
    tabSelectPromise = waitForEvent(Elements.tabList, "TabSelect");
    Browser.selectedTab = lastTab;
    yield tabSelectPromise;

    yield waitForCondition(()=>!SelectionHelperUI.isSelectionUIVisible);

    Browser.closeTab(Browser.selectedTab, { forceClose: true });
  },
  tearDown: setUpAndTearDown
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  runTests();
}
