/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time.
// Includes tests for dragging between domains, windows and iframes.

"use strict";

const kBaseUrl = "https://example.org/browser/dom/events/test/";

let testName;

requestLongerTimeout(2);

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
        aRequest.requestToken,
        aRequest.userActionId
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
    outerURL1: kBaseUrl + "browser_dragdrop_outer.html",
    outerURL2: kBaseUrl + "browser_dragdrop_outer.html",
    innerURL1: kBaseUrl + "browser_dragdrop_inner.html",
    innerURL2: kBaseUrl + "browser_dragdrop_inner.html",
  });
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);

const TEST_MODES = Object.freeze({
  ALLOW_PLAINTEXTONLY: {
    caAllow: true,
    turnOffPref: false,
    plainTextOnly: true,
    bypassForSameTab: false,
    // Since plain text only is being analyzed and the drag/drop tests don't
    // drag anything with a text/plain MIME type, we do not expect to run CA.
    shouldRunCA: false,
  },
  ALLOW_CHECKALLFORMATS: {
    caAllow: true,
    turnOffPref: false,
    plainTextOnly: false,
    bypassForSameTab: false,
    shouldRunCA: true,
  },
  BLOCK: {
    caAllow: false,
    turnOffPref: false,
    plainTextOnly: false,
    bypassForSameTab: false,
    shouldRunCA: true,
  },
  PREFOFF: {
    caAllow: true,
    turnOffPref: true,
    plainTextOnly: false,
    bypassForSameTab: false,
    shouldRunCA: false,
  },
  BYPASS_SAME_TAB: {
    caAllow: true,
    turnOffPref: false,
    plainTextOnly: false,
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
    TEST_MODES.ALLOW_PLAINTEXTONLY,
    TEST_MODES.ALLOW_CHECKALLFORMATS,
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

    let shouldRunCA = testMode.shouldRunCA;
    let description;
    if (testMode.bypassForSameTab) {
      shouldRunCA = !isSameTab;
      mockCA.caShouldAllow = isSameTab;
      description = `${isSameTab ? "no_run" : "diff_tab_deny"}_ca_with_same_tab_pref`;
      await SpecialPowers.pushPrefEnv({
        set: [["browser.contentanalysis.bypass_for_same_tab_operations", true]],
      });
    } else if (testMode.turnOffPref) {
      description = "no_run_ca_because_of_dnd_interception_point_pref";
    } else {
      description =
        (testMode.caAllow ? "allow_drop" : "deny_drop") +
        (testMode.plainTextOnly ? "_plain_text_only" : "");
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
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.contentanalysis.interception_point.drag_and_drop.plain_text_only",
          testMode.plainTextOnly,
        ],
      ],
    });

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
      `[${testName}]| Called AnalyzeContentRequestPrivate correct number of times`
    );
    is(
      mockCA.numGetURIForDropEvent,
      1,
      `[${testName}]| GetURIForDropEvent was called correct number of times`
    );
    if (testMode.turnOffPref || testMode.bypassForSameTab) {
      await SpecialPowers.popPrefEnv();
    }
    await SpecialPowers.popPrefEnv();
  }
};
