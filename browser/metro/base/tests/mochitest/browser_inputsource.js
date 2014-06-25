// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*=============================================================================
  Test cases
=============================================================================*/

function test() {
  runTests();
}

function setUp() {
  yield addTab("about:blank");
}

function testState(aState) {
  let bcastValue = document.getElementById("bcast_preciseInput").getAttribute("input");
  is(bcastValue, aState, "bcast attribute is " + aState);

  if (aState == "precise") {
    ok(InputSourceHelper.isPrecise, "InputSourceHelper");
    let uri = Util.makeURI("chrome://browser/content/cursor.css");
    ok(!StyleSheetSvc.sheetRegistered(uri, Ci.nsIStyleSheetService.AGENT_SHEET), "cursor stylesheet registered");
  } else {
    ok(!InputSourceHelper.isPrecise, "InputSourceHelper");
    let uri = Util.makeURI("chrome://browser/content/cursor.css");
    ok(StyleSheetSvc.sheetRegistered(uri, Ci.nsIStyleSheetService.AGENT_SHEET), "cursor stylesheet registered");
  }
}


gTests.push({
  desc: "precise/imprecise input switcher",
  setUp: setUp,
  run: function () {
    notifyPrecise();
    testState("precise");
    notifyImprecise();
    testState("imprecise");
    notifyPrecise();
    testState("precise");
    notifyImprecise();
    testState("imprecise");
  }
});

