/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time.
// Supports dragging between domains, windows and iframes.
// To avoid symmetric tests, the drag source must be in the first window.
// iframe domains must match outer frame domains.

// This file should be included in DND tests that define its parameters.
// That file needs to do the following:
// 1. Call await setup(configParams), where configParams is an object that
//    configures the specific test.  See `setup()` below.
// 2. Use Services.scriptloader.loadSubScript to load this script.
// 3. Define runTest(testName, srcBrowsingCxt, tgtBrowsingCxt, dndOptions),
//    which defaults to `runDnd` below.  Other definitions will typically
//    also need to delegate some functionality to runDnd.

"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

// The tabs, each in their own widget.
let tab1Cxt;
let tab2Cxt;

let dragServiceCid;

// JS controller for mock drag service
let dragController;

async function runDnd(
  testName,
  sourceBrowsingCxt,
  targetBrowsingCxt,
  dndOptions = {}
) {
  return EventUtils.synthesizeMockDragAndDrop({
    dragController,
    srcElement: "dropSource",
    targetElement: "dropTarget",
    sourceBrowsingCxt,
    targetBrowsingCxt,
    id: SpecialPowers.Ci.nsIDOMWindowUtils.DEFAULT_MOUSE_POINTER_ID,
    contextLabel: testName,
    info,
    record,
    dragAction: Ci.nsIDragService.DRAGDROP_ACTION_MOVE,
    ...dndOptions,
  });
}

async function openWindow(tabIdx, configParams) {
  let win =
    tabIdx == 0 ? window : await BrowserTestUtils.openNewBrowserWindow();
  const OUTER_URL_ARRAY = [configParams.outerURL1, configParams.outerURL2];
  let url = OUTER_URL_ARRAY[tabIdx];
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url,
  });
  registerCleanupFunction(async function () {
    await BrowserTestUtils.removeTab(tab);
    if (tabIdx != 0) {
      await BrowserTestUtils.closeWindow(win);
    }
  });

  // Set the URL for the iframe.  Also set
  // neverAllowSessionIsSynthesizedForTests for both frames
  // (the second is redundant if they are in the same process).
  const INNER_URL_ARRAY = [configParams.innerURL1, configParams.innerURL2];
  await SpecialPowers.spawn(
    tab.linkedBrowser.browsingContext,
    [tabIdx, INNER_URL_ARRAY[tabIdx]],
    async (tabIdx, iframeUrl) => {
      let iframe = content.document.getElementById("iframe");
      if (!iframe && content.document.body.shadowRoot) {
        iframe = content.document.body.shadowRoot.getElementById("iframe");
      }
      ok(iframe, `Found iframe in window ${tabIdx}`);
      let loadedPromise = new Promise(res => {
        iframe.addEventListener("load", res, { once: true });
      });
      iframe.src = iframeUrl;
      await loadedPromise;
      const ds = SpecialPowers.Cc[
        "@mozilla.org/widget/dragservice;1"
      ].getService(SpecialPowers.Ci.nsIDragService);
      ds.neverAllowSessionIsSynthesizedForTests = true;
    }
  );

  await SpecialPowers.spawn(
    tab.linkedBrowser.browsingContext.children[0],
    [],
    () => {
      const ds = SpecialPowers.Cc[
        "@mozilla.org/widget/dragservice;1"
      ].getService(SpecialPowers.Ci.nsIDragService);
      ds.neverAllowSessionIsSynthesizedForTests = true;
    }
  );

  return tab.linkedBrowser.browsingContext;
}

/**
 * Configure the test.  All fields are required.
 *
 * @param {Object} configParams
 * @param {String} configParams.outerURL1
 *                 URL of window #1's main frame's content (not iframes).
 *                 The document must contain an iframe with ID "iframe"
 *                 that will be used to load the iframe HTML.  It also
 *                 must contain an element with ID "dropSource" that will be
 *                 used as the source of a drag, as well as one with ID
 *                 "dropTarget" that will be used as the drop target.
 * @param {String} configParams.outerURL2
 *                 Like outerURL1 but for the second window.  outerURL1 and
 *                 outerURL2 may be identical.  Must include "dropSource" and
 *                 "dropTarget" elements.
 * @param {String} configParams.innerURL1
 *                 URL of the inner frame's content in window #1.  Must
 *                 include "dropSource" and "dropTarget" elements.
 * @param {String} configParams.innerURL1
 *                 URL of the inner frame's content in window #2.  Must
 *                 include "dropSource" and "dropTarget" elements.
 */
async function setup(configParams) {
  const oldDragService = SpecialPowers.Cc[
    "@mozilla.org/widget/dragservice;1"
  ].getService(SpecialPowers.Ci.nsIDragService);
  dragController = oldDragService.getMockDragController();
  dragServiceCid = MockRegistrar.register(
    "@mozilla.org/widget/dragservice;1",
    dragController.mockDragService
  );
  ok(dragServiceCid, "MockDragService was registered");
  // If the mock failed then don't continue or else we could trigger native
  // DND behavior.
  if (!dragServiceCid) {
    SimpleTest.finish();
  }
  registerCleanupFunction(async function () {
    MockRegistrar.unregister(dragServiceCid);
  });
  dragController.mockDragService.neverAllowSessionIsSynthesizedForTests = true;

  tab1Cxt = await openWindow(0, configParams);
  tab2Cxt = await openWindow(1, configParams);
}

// ----------------------------------------------------------------------------
// Test dragging between different frames and different domains
// ----------------------------------------------------------------------------
// Define runTest to establish a test between two (possibly identical) contexts.
// runTest has the same signature as runDnd.
var runTest = runDnd;

add_task(async function test_dnd_tab1_to_tab1() {
  await runTest("tab1->tab1", tab1Cxt, tab1Cxt);
});

add_task(async function test_dnd_tab1_to_iframe1() {
  await runTest("tab1->iframe1", tab1Cxt, tab1Cxt.children[0]);
});

add_task(async function test_dnd_tab1_to_tab2() {
  await runTest("tab1->tab2", tab1Cxt, tab2Cxt);
});

add_task(async function test_dnd_tab1_to_iframe2() {
  await runTest("tab1->iframe2", tab1Cxt, tab2Cxt.children[0]);
});

add_task(async function test_dnd_iframe1_to_tab1() {
  await runTest("iframe1->tab1", tab1Cxt.children[0], tab1Cxt);
});

add_task(async function test_dnd_iframe1_to_iframe1() {
  await runTest("iframe1->iframe1", tab1Cxt.children[0], tab1Cxt.children[0]);
});

add_task(async function test_dnd_iframe1_to_tab2() {
  await runTest("iframe1->tab2", tab1Cxt.children[0], tab2Cxt);
});

add_task(async function test_dnd_iframe1_to_iframe2() {
  await runTest("iframe1->iframe2", tab1Cxt.children[0], tab2Cxt.children[0]);
});
