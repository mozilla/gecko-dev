/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Wraps the given object in an XPConnect wrapper and, if an interface
// is passed, queries the result to that interface.
function xpcWrap(obj, iface) {
  let ifacePointer = Cc[
    "@mozilla.org/supports-interface-pointer;1"
  ].createInstance(Ci.nsISupportsInterfacePointer);

  ifacePointer.data = obj;
  if (iface) {
    return ifacePointer.data.QueryInterface(iface);
  }
  return ifacePointer.data;
}

/**
 * Mock a (set of) service(s) as the object mockService.
 *
 * @param {[string]} serviceNames
 *                   array of services names that mockService will be
 *                   allowed to QI to.  Must include the name of the
 *                   service referenced by contractId.
 * @param {string}   contractId
 *                   the component ID that will reference the mock object
 *                   instead of the original service
 * @param {object}   interfaceObj
 *                   interface object for the component
 * @param {object}   mockService
 *                   object that satisfies the contract well
 *                   enough to use as a mock of it
 * @returns {object} The newly-mocked service
 */
function mockService(serviceNames, contractId, interfaceObj, mockService) {
  // xpcWrap allows us to mock [implicit_jscontext] methods.
  let newService = {
    ...mockService,
    QueryInterface: ChromeUtils.generateQI(serviceNames),
  };
  let o = xpcWrap(newService, interfaceObj);
  const { MockRegistrar } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistrar.sys.mjs"
  );
  let cid = MockRegistrar.register(contractId, o);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(cid);
  });
  return newService;
}

/**
 * Mock the nsIContentAnalysis service with the object mockCAService.
 *
 * @param {object}    mockCAServiceTemplate
 *                    the mock nsIContentAnalysis template object
 * @returns {object}  The newly-mocked service that integrates the template
 */
async function mockContentAnalysisService(mockCAServiceTemplate) {
  // Some of the C++ code that tests if CA is active checks this
  // pref (even though it would perhaps be better to just ask
  // nsIContentAnalysis)
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentanalysis.enabled", true]],
  });
  registerCleanupFunction(async function () {
    SpecialPowers.popPrefEnv();
  });
  let realCAService = SpecialPowers.Cc[
    "@mozilla.org/contentanalysis;1"
  ].getService(SpecialPowers.Ci.nsIContentAnalysis);
  let mockCAService = mockService(
    ["nsIContentAnalysis"],
    "@mozilla.org/contentanalysis;1",
    Ci.nsIContentAnalysis,
    mockCAServiceTemplate
  );
  if (mockCAService) {
    mockCAService.realCAService = realCAService;
  }
  return mockCAService;
}

async function waitForFileToAlmostMatchSize(filePath, expectedSize) {
  // In Cocoa the CGContext adds a hash, plus there are other minor
  // non-user-visible differences, so we need to be a bit more sloppy there.
  //
  // We see one byte difference in Windows and Linux on automation sometimes,
  // though files are consistently the same locally, that needs
  // investigation, but it's probably harmless.
  // Note that this is copied from browser_print_stream.js.
  const maxSizeDifference = AppConstants.platform == "macosx" ? 100 : 3;

  // Buffering shenanigans? Wait for sizes to match... There's no great
  // IOUtils methods to force a flush without writing anything...
  // Note that this means if this results in a timeout this is exactly
  // the same as a test failure.
  // This is taken from toolkit/components/printing/tests/browser_print_stream.js
  await TestUtils.waitForCondition(async function () {
    let fileStat = await IOUtils.stat(filePath);

    info("got size: " + fileStat.size + " expected: " + expectedSize);
    Assert.greater(
      fileStat.size,
      0,
      "File should not be empty: " + fileStat.size
    );
    return Math.abs(fileStat.size - expectedSize) <= maxSizeDifference;
  }, "Sizes should (almost) match");
}

function makeMockContentAnalysis() {
  return {
    isActive: true,
    mightBeActive: true,
    errorValue: undefined,
    waitForEventToFinish: false,
    // This is a dummy event target that uses custom events for bidirectional
    // communication between the individual test and the mock CA object.
    // Events are:
    //   inAnalyzeContentRequest:
    //     If waitForEvent was true, this is sent by mock CA when its
    //     AnalyzeContentRequest is ready to issue a response.  It will wait
    //     for returnContentAnalysisResponse to be received before issuing
    //     the response.
    //  returnContentAnalysisResponse:
    //     If waitForEvent was true, this must be sent by the test to tell
    //     AnalyzeContentRequest to issue its response.
    eventTarget: new EventTarget(),

    setupForTest(shouldAllowRequest, waitForEvent) {
      this.shouldAllowRequest = shouldAllowRequest;
      this.errorValue = undefined;
      this.waitForEvent = !!waitForEvent;
      this.clearCalls();
    },

    setupForTestWithError(errorValue) {
      this.errorValue = errorValue;
      this.clearCalls();
    },

    clearCalls() {
      this.calls = [];
      this.browsingContextsForURIs = [];
      this.agentCancelCalls = 0;
    },

    getAction() {
      if (this.shouldAllowRequest === undefined) {
        this.shouldAllowRequest = true;
      }
      return this.shouldAllowRequest
        ? Ci.nsIContentAnalysisResponse.eAllow
        : Ci.nsIContentAnalysisResponse.eBlock;
    },

    // nsIContentAnalysis methods

    // Use the real counterparts of all public analyze* methods.
    // They will in turn call our mock analyzeContentRequestPrivate.
    analyzeContentRequests(requests, autoAcknowledge) {
      return this.realCAService.analyzeContentRequests(
        requests,
        autoAcknowledge
      );
    },
    analyzeContentRequestsCallback(requests, autoAcknowledge, callback) {
      if (this.errorValue) {
        if (requests.length != 1) {
          // Sanity testing the test.  Exception-expecting tests don't send
          // multiple requests.  Don't clutter the log unless we fail.
          is(
            requests.length,
            1,
            "Test framework doesn't support throwing an exception from a multipart request"
          );
        }
        // If we throw in analyzeContentRequestPrivate then this function is
        // a lower stack frame and generates an additional test failure that
        // we can't tell it to expect.
        // The test framework expects the user action and request token to
        // be set anyway.  The values don't matter.
        // We are also required to call the callback.
        requests[0].userActionId = "user-action-for-error";
        requests[0].userActionRequestsCount = 1;
        requests[0].requestToken = "request-token-for-error";
        this.calls.push(requests[0]);
        callback.error(this.errorValue);
        throw this.errorValue;
      }
      this.realCAService.analyzeContentRequestsCallback(
        requests,
        autoAcknowledge,
        callback
      );
    },

    analyzeContentRequestPrivate(request, _autoAcknowledge, callback) {
      info(
        "Mock ContentAnalysis service: analyzeContentRequestPrivate, this.shouldAllowRequest=" +
          this.shouldAllowRequest +
          ", this.waitForEvent=" +
          this.waitForEvent
      );
      info(
        `  Request type: ${request.analysisType} ` +
          `| reason: ${request.reason} ` +
          `| operation: ${request.operationTypeForDisplay} ` +
          `| operation string: '${request.operationDisplayString}'`
      );
      info(
        `  Text content: '${request.textContent}' ` +
          `| filePath: '${request.filePath}' ` +
          `| printDataHandle: ${request.printDataHandle} ` +
          `| printDataSize: ${request.printDataSize}`
      );
      info(
        `  Printer name: '${request.printerName}' ` +
          `| url: '${request.url ? request.url.spec : ""}' ` +
          `| Request token: ${request.requestToken} ` +
          `| user action ID: ${request.userActionId} ` +
          `| user action count: ${request.userActionRequestsCount}`
      );
      if (this.errorValue) {
        // This is just sanity testing the test framework.  Only report if it
        // fails.
        ok(
          !this.errorValue,
          "can't throw an exception in mock analyzeContentRequestPrivate"
        );
      }

      this.calls.push(request);

      // Use setTimeout to simulate an async activity.
      setTimeout(async () => {
        if (this.waitForEvent) {
          let waitPromise = new Promise(res => {
            this.eventTarget.addEventListener(
              "returnContentAnalysisResponse",
              () => {
                res();
              },
              { once: true }
            );
          });
          this.eventTarget.dispatchEvent(
            new CustomEvent("inAnalyzeContentRequest")
          );
          await waitPromise;
        }

        let response = this.realCAService.makeResponseForTest(
          this.getAction(),
          request.requestToken,
          request.userActionId
        );
        callback.contentResult(response);
      }, 0);
    },

    cancelAllRequests() {
      // This is called on exit, no need to do anything
    },

    getURIForBrowsingContext(aBrowsingContext) {
      this.browsingContextsForURIs.push(aBrowsingContext);
      return this.realCAService.getURIForBrowsingContext(aBrowsingContext);
    },

    setCachedResponse(aURI, aClipboardSequenceNumber, aAction) {
      return this.realCAService.setCachedResponse(
        aURI,
        aClipboardSequenceNumber,
        aAction
      );
    },

    getCachedResponse(aURI, aClipboardSequenceNumber, aAction, aIsValid) {
      return this.realCAService.getCachedResponse(
        aURI,
        aClipboardSequenceNumber,
        aAction,
        aIsValid
      );
    },

    showBlockedRequestDialog(aRequest) {
      info(`got showBlockedRequestDialog for request ${aRequest.requestToken}`);
    },

    sendCancelToAgent(aUserActionId) {
      info(`got sendCancelToAgent for user action ID ${aUserActionId}`);
      this.agentCancelCalls = this.agentCancelCalls + 1;
    },
  };
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  return new Promise(resolve => {
    whenTabLoaded(aTab, resolve);
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param {object} tab
 *        The tab to load into.
 * @param {string} [url]
 *        The url to load, or the current url.
 * @returns {Promise<string>} resolved when the event is handled. Rejected if
 *          a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  }

  return loaded;
}

function promisePopupShown(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "hidden");
}
