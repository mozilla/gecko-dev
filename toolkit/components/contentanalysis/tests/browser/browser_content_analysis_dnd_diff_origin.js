/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time.
// Includes tests for dragging between domains, windows and iframes.

"use strict";

const kBaseUrl1 = "https://example.org/browser/dom/events/test/";
const kBaseUrl2 = "https://example.com/browser/dom/events/test/";

let testName;

let mockCA = {
  isActive: true,
  mightBeActive: true,
  caShouldAllow: undefined,
  numAnalyzeContentRequestPrivateCalls: undefined,
  numGetURIForDropEvent: undefined,

  getURIForDropEvent(event) {
    info(`[${testName}]| Called getURIForDropEvent`);
    this.numGetURIForDropEvent += 1;
    return this.realCAService.getURIForDropEvent(event);
  },

  analyzeContentRequestPrivate(aRequest, _aAutoAcknowledge, aCallback) {
    info(`[${testName}]| Called analyzeContentRequestPrivate`);
    this.numAnalyzeContentRequestPrivateCalls += 1;

    is(
      aRequest.analysisType,
      Ci.nsIContentAnalysisRequest.eBulkDataEntry,
      "request has correct analysisType"
    );
    is(
      aRequest.reason,
      Ci.nsIContentAnalysisRequest.eDragAndDrop,
      "request has correct reason"
    );
    is(
      aRequest.operationTypeForDisplay,
      Ci.nsIContentAnalysisRequest.eDroppedText,
      "request has correct operation type"
    );
    is(
      aRequest.userActionRequestsCount,
      1,
      "request has correct userActionRequestsCount"
    );
    ok(
      aRequest.userActionId.length,
      "request userActionId should not be empty"
    );

    aCallback.contentResult(
      this.realCAService.makeResponseForTest(
        this.caShouldAllow
          ? Ci.nsIContentAnalysisResponse.eAllow
          : Ci.nsIContentAnalysisResponse.eBlock,
        aRequest.requestToken
      )
    );
  },

  analyzeContentRequests(aRequests, aAutoAcknowledge) {
    // This will call into our mock analyzeContentRequestPrivate
    return this.realCAService.analyzeContentRequests(
      aRequests,
      aAutoAcknowledge
    );
  },

  showBlockedRequestDialog(aRequest) {
    info(`got showBlockedRequestDialog for request ${aRequest.requestToken}`);
  },
};

add_setup(async function () {
  mockCA = await mockContentAnalysisService(mockCA);

  await setup({
    outerURL1: kBaseUrl1 + "browser_dragdrop_outer.html",
    outerURL2: kBaseUrl2 + "browser_dragdrop_outer.html",
    innerURL1: kBaseUrl1 + "browser_dragdrop_inner.html",
    innerURL2: kBaseUrl2 + "browser_dragdrop_inner.html",
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
    bypassForSameTab: false,
    shouldRunCA: true,
  },
  BLOCK: {
    caAllow: false,
    turnOffPref: false,
    bypassForSameTab: false,
    shouldRunCA: true,
  },
  PREFOFF: {
    caAllow: true,
    turnOffPref: true,
    bypassForSameTab: false,
    shouldRunCA: false,
  },
  BYPASS_SAME_TAB: {
    caAllow: true,
    turnOffPref: false,
    bypassForSameTab: true,
    shouldRunCA: false,
  },
});

runTest = async function (
  testRootName,
  sourceBrowsingCxt,
  targetBrowsingCxt,
  dndOptions = {}
) {
  mockCA.sourceBrowsingCxt = sourceBrowsingCxt;
  mockCA.targetBrowsingCxt = targetBrowsingCxt;

  for (let testMode of [
    TEST_MODES.ALLOW,
    TEST_MODES.BLOCK,
    TEST_MODES.PREFOFF,
    TEST_MODES.BYPASS_SAME_TAB,
  ]) {
    let isSameTab =
      sourceBrowsingCxt.top == targetBrowsingCxt.top &&
      targetBrowsingCxt.currentWindowGlobal.documentPrincipal.subsumes(
        sourceBrowsingCxt.currentWindowGlobal.documentPrincipal
      );

    mockCA.caShouldAllow = testMode.caAllow;
    mockCA.numAnalyzeContentRequestPrivateCalls = 0;
    mockCA.numGetURIForDropEvent = 0;

    let shouldRunCA;
    let description;
    if (testMode.shouldRunCA) {
      shouldRunCA = true;
      description = testMode.caAllow ? "allow_drop" : "deny_drop";
    } else if (testMode.turnOffPref) {
      shouldRunCA = false;
      description = "no_run_ca_because_of_dnd_interception_point_pref";
    } else {
      if (!testMode.bypassForSameTab) {
        // Sanity testing the test -- don't log anything unless we fail.
        ok(
          testMode.bypassForSameTab,
          "Expected testMode to be handled already"
        );
      }
      shouldRunCA = !isSameTab;
      mockCA.caShouldAllow = isSameTab;
      description = `${isSameTab ? "no_run" : "diff_tab_deny"}_ca_with_same_tab_pref`;
      await SpecialPowers.pushPrefEnv({
        set: [["browser.contentanalysis.bypass_for_same_tab_operations", true]],
      });
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

    let dropPromise = SpecialPowers.spawn(
      targetBrowsingCxt,
      [mockCA.caShouldAllow],
      async shouldAllow => {
        let resolver;
        let promise = new Promise(res => {
          resolver = res;
        });
        let targetElt = content.document.getElementById("dropTarget");
        targetElt.addEventListener(
          shouldAllow ? "drop" : "dragleave",
          _ => {
            resolver();
          },
          { once: true }
        );
        await promise;
        info("dropPromise was alerted in content");
      }
    );

    await runDnd(name, sourceBrowsingCxt, targetBrowsingCxt, {
      dropPromise,
      expectDragLeave: !mockCA.caShouldAllow,
      ...dndOptions,
    });

    is(
      mockCA.numAnalyzeContentRequestPrivateCalls,
      shouldRunCA ? 1 : 0,
      `[${testName}]| Called analyzeContentRequestPrivate correct number of times`
    );
    is(
      mockCA.numGetURIForDropEvent,
      1,
      `[${testName}]| GetURIForDropEvent was called correct number of times`
    );
    if (testMode.turnOffPref || testMode.bypassForSameTab) {
      await SpecialPowers.popPrefEnv();
    }
  }
};
