/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function disableSpeculativeConnections() {
  // Disable speculative connections so the test doesn't leak due to finishing before the connection
  // closes on its own.
  registerCleanupFunction(() => Services.prefs.clearUserPref("network.http.speculative-parallel-limit"));
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
}


function test()
{
  // Don't use about:home as the homepage for new windows
  // Services.prefs.setIntPref("browser.startup.page", 0);
  // registerCleanupFunction(function() Services.prefs.clearUserPref("browser.startup.page"));
  // Services.prefs.setCharPref("browser.search.selectedEngine", "");
  // registerCleanupFunction(function() Services.prefs.clearUserPref("browser.search.selectedEngine"));
  // Services.prefs.setBoolPref("browser.search.update", false);
  // registerCleanupFunction(function() Services.prefs.clearUserPref("browser.search.update"));
  // gBrowser.selectedTab.ownerDocument.querySelector("#search-container").hidden = true;
  // disableSpeculativeConnections();
  // registerCleanupFunction(() => info(gScratchpadWindow));
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests, { noFocus: false });
  }, true);

  content.location = "data:text/html;charset=utf8,test Scratchpad pretty print error goto line.";
  // content.location = "data:text/xml;charset=UTF-8,<?xml version='1.0'?>" +
  //   "<?xml-stylesheet href='chrome://global/skin/global.css'?>" +
  //   "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
  //   " title='Editor' width='600' height='500'><box flex='1'/></window>";
  // info(content.location);
}

function testJumpToPrettyPrintError(sp, error, remark) {
  info("will test jumpToLine after prettyPrint error" + remark);
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    is(/Invalid regexp flag \(3:10\)/.test(error), true, "prettyPrint expects error in editor text:\n" + error);
    let [ , errorLine, errorColumn ] = error.match(/\((\d+):(\d+)\)/);
    let editorDoc = sp.editor.container.contentDocument;
    sp.editor.jumpToLine();
    let lineInput = editorDoc.querySelector("input");
    let errorLocation = lineInput.value;
    let [ inputLine, inputColumn ] = errorLocation.split(":");
    is(inputLine, errorLine, "jumpToLine input field is set from editor selection (line)");
    is(inputColumn, errorColumn, "jumpToLine input field is set from editor selection (column)");
    EventUtils.synthesizeKey("VK_RETURN", { }, editorDoc.defaultView);
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    let cursor = sp.editor.getCursor();
    is(inputLine, cursor.line + 1, "jumpToLine goto error location (line)");
    is(inputColumn, cursor.ch + 1, "jumpToLine goto error location (column)");
}

function runTests(sw, sp)
{
  sp.setText([
    "// line 1",
    "// line 2",
    "var re = /a bad /regexp/; // line 3 is an obvious syntax error!",
    "// line 4",
    "// line 5",
    ""].join("\n"));
  info("\nsw.window.location.href\n" + sw.window.location.href);
  info("\nsp.editor.container.contentDocument.location.href\n" + sp.editor.container.contentDocument.location.href);
  // info("\nsp.editor.container.contentDocument.location\n" + Object.getOwnPropertyNames(sp.editor.container.contentDocument.location));
  // info("\nsw.window\n" + Object.getOwnPropertyNames(sw.window));
  sp.prettyPrint().then(aFulfill => {
    ok(false, "Expecting Invalid regexp flag (3:10)");
    finishTest();
  }, error => {
    testJumpToPrettyPrintError(sp, error, " (Bug 1005471, first time)");
    info(sp.getText());
    let [ from, to ] = sp.editor.getPosition(0, sp.getText().length);
    // finishTest();
    // Cannot setSeletion although from, to are OK.
    // sp.editor.cm.setSelection(from, to);
  });
  sp.prettyPrint().then(aFulfill => {
    ok(false, "Expecting Invalid regexp flag (3:10)");
    finishTest();
  }, error => {
    // Second time verifies bug in earlier implementation fixed.
    testJumpToPrettyPrintError(sp, error, " (second time)");
    info(sp.getText());
    let [ from, to ] = sp.editor.getPosition(0, sp.getText().length);
    finishTest();
    // Cannot setSeletion although from, to are OK.
    // sp.editor.cm.setSelection(from, to);
  });
  // sp.editor.setSelection(sp.editor.getPosition(0, sp.getText().length)).then(() => {
  //   info("setSelection");
  // // sp.prettyPrint().then(aFulfill => {
  // //   ok(false, "Expecting Invalid regexp flag (3:10)");
  // //   finishTest();
  // // }, error => {
  // //   // sw.removeEventListener("focus", SimpleTest.waitForEvent, true);
  //   testJumpToPrettyPrintError(sp, error);
  // // });
  //   // finishTest();
  // }, () => {
  //   ok(false, "setSelection");
  //   finishTest();
  // });
}

function finishTest(sw)
{
  finish();
  // calling info on some object: Workaround to eliminate
  // 0:24.00 TEST-UNEXPECTED-FAIL | chrome://mochitests/content/browser/browser/devtools/scratchpad/test/browser_scratchpad_pprint_error_goto_line.js | leaked until shutdown [nsGlobalWindow #21 inner chrome://browser/content/devtools/scratchpad.xul about:blank]
  // 0:24.00 TEST-UNEXPECTED-FAIL | chrome://mochitests/content/browser/browser/devtools/scratchpad/test/browser_scratchpad_pprint_error_goto_line.js | leaked until shutdown [nsGlobalWindow #20 outer  about:blank]
  info(gScratchpadWindow);
}
