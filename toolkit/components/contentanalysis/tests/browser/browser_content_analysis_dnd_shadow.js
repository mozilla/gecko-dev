/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Drag and drop stuff from inside one shadow DOM to another shadow DOM.
// Includes shadow DOMs in iframes.

"use strict";

const kBaseUrl1 = "https://example.org/browser/dom/events/test/";
const kBaseUrl2 = "https://example.com/browser/dom/events/test/";

// Resolve fn for promise we resolve after mockCA.analyzeContentRequest runs.
let resolveDropPromise;

let testName;

let mockCA = {
  isActive: true,
  mightBeActive: true,
  caShouldAllow: undefined,
  numAnalyzeContentRequestCalls: undefined,
  numGetURIForDropEvent: undefined,

  getURIForDropEvent(event) {
    info(`[${testName}]| Called getURIForDropEvent`);
    this.numGetURIForDropEvent += 1;
    return this.realCAService.getURIForDropEvent(event);
  },

  async analyzeContentRequest(_aRequest, _aAutoAcknowledge) {
    info(`[${testName}]| Called analyzeContentRequest`);
    this.numAnalyzeContentRequestCalls += 1;

    // We want analyzeContentRequest to return before dropPromise is resolved
    // because dropPromise tells the test harness that it is time to check that
    // the drop or dragleave event was received, and that is sent immediately
    // after analyzeContentRequest returns (as part of a promise handler chain).
    setTimeout(resolveDropPromise, 0);
    return { shouldAllowContent: this.caShouldAllow };
  },
};

add_setup(async function () {
  mockCA = await mockContentAnalysisService(mockCA);

  await setup({
    outerURL1: kBaseUrl1 + "browser_dragdrop_shadow_outer.html",
    outerURL2: kBaseUrl2 + "browser_dragdrop_shadow_outer.html",
    innerURL1: kBaseUrl1 + "browser_dragdrop_shadow_inner.html",
    innerURL2: kBaseUrl2 + "browser_dragdrop_shadow_inner.html",
  });
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);

const TEST_MODES = Object.freeze({
  ALLOW: {
    caAllow: true,
    turnOffPref: false,
    shouldDrag: true,
    shouldRunCA: true,
  },
  BLOCK: {
    caAllow: false,
    turnOffPref: false,
    shouldDrag: false,
    shouldRunCA: true,
  },
  PREFOFF: {
    caAllow: false,
    turnOffPref: true,
    shouldDrag: true,
    shouldRunCA: false,
  },
});

runTest = async function (
  testRootName,
  sourceBrowsingCxt,
  targetBrowsingCxt,
  dndOptions = {}
) {
  if (
    sourceBrowsingCxt.top == targetBrowsingCxt.top &&
    targetBrowsingCxt.currentWindowGlobal.documentPrincipal.subsumes(
      sourceBrowsingCxt.currentWindowGlobal.documentPrincipal
    )
  ) {
    // Content Analysis should not run.
    info(testRootName);
    testName = testRootName;
    mockCA.numAnalyzeContentRequestCalls = 0;
    mockCA.numGetURIForDropEvent = 0;
    await runDnd(testRootName, sourceBrowsingCxt, targetBrowsingCxt, {
      ...dndOptions,
    });
    is(
      mockCA.numAnalyzeContentRequestCalls,
      0,
      `[${testName}]| AnalyzeContentRequest was not called`
    );
    is(
      mockCA.numGetURIForDropEvent,
      0,
      `[${testName}]| GetURIForDropEvent was not called`
    );
    return;
  }

  for (let testMode of [
    TEST_MODES.ALLOW,
    TEST_MODES.BLOCK,
    TEST_MODES.PREFOFF,
  ]) {
    let description;
    if (testMode.shouldRunCA) {
      description = testMode.caAllow ? "allow_drop" : "deny_drop";
    } else {
      description = "no_run_ca_because_of_pref";
    }
    let name = `${testRootName}:${description}`;
    info(name);
    testName = name;
    if (testMode.turnOffPref) {
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "browser.contentanalysis.interception_point.drag_and_drop.enabled",
            false,
          ],
        ],
      });
    }
    mockCA.caShouldAllow = testMode.caAllow;
    mockCA.numAnalyzeContentRequestCalls = 0;
    mockCA.numGetURIForDropEvent = 0;
    let dropPromise = new Promise(res => {
      if (testMode.shouldRunCA) {
        resolveDropPromise = res;
      } else {
        // CA won't get called, just resolve the promise now
        res();
      }
    });
    await runDnd(name, sourceBrowsingCxt, targetBrowsingCxt, {
      dropPromise,
      expectDragLeave: !testMode.shouldDrag,
      ...dndOptions,
    });
    const expectedCaCalls = testMode.shouldRunCA ? 1 : 0;
    is(
      mockCA.numAnalyzeContentRequestCalls,
      expectedCaCalls,
      `[${testName}]| Called AnalyzeContentRequest correct number of times`
    );
    is(
      mockCA.numGetURIForDropEvent,
      expectedCaCalls,
      `[${testName}]| GetURIForDropEvent was called correct number of times`
    );
    if (testMode.turnOffPref) {
      await SpecialPowers.popPrefEnv();
    }
  }
};
