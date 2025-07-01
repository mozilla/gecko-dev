/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

// We are borrowing tests from the netmonitor frontend
const NETMONITOR_TEST_FOLDER =
  "https://example.com/browser/devtools/client/netmonitor/test/";
const CSP_URL = `${NETMONITOR_TEST_FOLDER}html_csp-test-page.html`;
const JS_CSP_URL = `${NETMONITOR_TEST_FOLDER}js_websocket-worker-test.js`;
const CSS_CSP_URL = `${NETMONITOR_TEST_FOLDER}internal-loaded.css`;

const CSP_BLOCKED_REASON_CODE = 4000;

add_task(async function testContentProcessRequests() {
  info(`Tests for NETWORK_EVENT resources fired from the content process`);

  const expectedNetworkEvents = [
    {
      url: CSP_URL,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
    {
      url: JS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
    {
      url: CSS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
  ];

  const expectedUpdates = {
    [CSP_URL]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
    [JS_CSP_URL]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
        responseEndAvailable: {
          responseEndAvailable: true,
        },
      },
    },
    [CSS_CSP_URL]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
  };

  await assertNetworkResourcesOnPage(
    CSP_URL,
    expectedNetworkEvents,
    expectedUpdates
  );
});

add_task(async function testCanceledRequest() {
  info(`Tests for NETWORK_EVENT resources with a canceled request`);

  // Do a XHR request that we cancel against a slow loading page
  const requestUrl =
    "https://example.org/document-builder.sjs?delay=1000&html=foo";
  const html =
    "<!DOCTYPE html><script>(" +
    function (xhrUrl) {
      const xhr = new XMLHttpRequest();
      xhr.open("GET", xhrUrl);
      xhr.send(null);
    } +
    ")(" +
    JSON.stringify(requestUrl) +
    ")</script>";
  const pageUrl =
    "https://example.org/document-builder.sjs?html=" + encodeURIComponent(html);

  const expectedNetworkEvents = [
    {
      url: pageUrl,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
    {
      url: requestUrl,
      method: "GET",
      isNavigationRequest: false,
      blockedReason: "NS_BINDING_ABORTED",
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
  ];

  const expectedUpdates = {
    [pageUrl]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
    [requestUrl]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
  };

  // Register a one-off listener to cancel the XHR request
  // Using XMLHttpRequest's abort() method from the content process
  // isn't reliable and would introduce many race condition in the test.
  // Canceling the request via nsIRequest.cancel privileged method,
  // from the parent process is much more reliable.
  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(subject) {
      subject = subject.QueryInterface(Ci.nsIHttpChannel);
      if (subject.URI.spec == requestUrl) {
        subject.cancel(Cr.NS_BINDING_ABORTED);
        Services.obs.removeObserver(observer, "http-on-modify-request");
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");

  await assertNetworkResourcesOnPage(
    pageUrl,
    expectedNetworkEvents,
    expectedUpdates
  );
});

add_task(async function testIframeRequest() {
  info(`Tests for NETWORK_EVENT resources with an iframe`);

  // Do a XHR request that we cancel against a slow loading page
  const iframeRequestUrl =
    "https://example.org/document-builder.sjs?html=iframe-request";
  const iframeHtml = `iframe<script>fetch("${iframeRequestUrl}")</script>`;
  const iframeUrl =
    "https://example.org/document-builder.sjs?html=" +
    encodeURIComponent(iframeHtml);
  const html = `top-document<iframe src="${iframeUrl}"></iframe>`;
  const pageUrl =
    "https://example.org/document-builder.sjs?html=" + encodeURIComponent(html);

  const expectedNetworkEvents = [
    // The top level navigation request relates to the previous top level target.
    // Unfortunately, it is hard to test because it is racy.
    // The target front might be destroyed and `targetFront.url` will be null.
    // Or not just yet and be equal to "about:blank".
    {
      url: pageUrl,
      method: "GET",
      chromeContext: false,
      isNavigationRequest: true,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
    {
      url: iframeUrl,
      method: "GET",
      isNavigationRequest: false,
      targetFrontUrl: pageUrl,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
    {
      url: iframeRequestUrl,
      method: "GET",
      isNavigationRequest: false,
      targetFrontUrl: iframeUrl,
      chromeContext: false,
      requestCookiesAvailable: true,
      requestHeadersAvailable: true,
    },
  ];

  const expectedUpdates = {
    [pageUrl]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
    [iframeUrl]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
    [iframeRequestUrl]: {
      responseStart: {
        status: "200",
        mimeType: "text/html",
        responseCookiesAvailable: true,
        responseHeadersAvailable: true,
        responseStartAvailable: true,
      },
      eventTimingsAvailable: {
        totalTime: 12,
        eventTimingsAvailable: true,
      },
      securityInfoAvailable: {
        securityState: "secure",
        isRacing: false,
        securityInfoAvailable: true,
      },
      responseContentAvailable: {
        contentSize: 200,
        transferredSize: 343,
        mimeType: "text/html",
        blockedReason: 0,
        responseContentAvailable: true,
      },
      responseEndAvailable: {
        responseEndAvailable: true,
      },
    },
  };

  await assertNetworkResourcesOnPage(
    pageUrl,
    expectedNetworkEvents,
    expectedUpdates
  );
});

async function assertNetworkResourcesOnPage(
  url,
  expectedNetworkEvents,
  expectedUpdates
) {
  // First open a blank document to avoid spawning any request
  const tab = await addTab("about:blank");

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  const matchedRequests = {};

  const onAvailable = resources => {
    for (const resource of resources) {
      // Immediately assert the resource, as the same resource object
      // will be notified to onUpdated and so if we assert it later
      // we will not highlight attributes that aren't set yet from onAvailable.
      if (matchedRequests[resource.url] !== undefined) {
        return;
      }
      const idx = expectedNetworkEvents.findIndex(e => e.url === resource.url);
      Assert.notEqual(
        idx,
        -1,
        "Found a matching available notification for: " + resource.url
      );
      // Track already matched resources in case there is many requests with the same url
      if (idx >= 0) {
        matchedRequests[resource.url] = 0;
      }

      assertNetworkResources(resource, expectedNetworkEvents[idx]);
    }
  };

  const onUpdated = updates => {
    for (const {
      resource,
      update: { resourceUpdates },
    } of updates) {
      const idx = expectedNetworkEvents.findIndex(e => e.url === resource.url);
      Assert.notEqual(
        idx,
        -1,
        "Found a matching available notification for the update: " +
          resource.url
      );

      matchedRequests[resource.url] = matchedRequests[resource.url] + 1;
      assertNetworkUpdateResources(
        resourceUpdates,
        expectedUpdates[resource.url]
      );
    }
  };

  // Start observing for network events before loading the test page
  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
  });

  // Load the test page that fires network requests
  const onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
  await onLoaded;

  // Make sure we processed all the expected request updates
  await waitFor(
    () => Object.keys(matchedRequests).length == expectedNetworkEvents.length,
    "Wait for all expected available notifications"
  );

  resourceCommand.unwatchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
  });

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
}

function assertNetworkResources(actual, expected) {
  is(
    actual.resourceType,
    ResourceCommand.TYPES.NETWORK_EVENT,
    "The resource type is correct"
  );
  is(
    typeof actual.innerWindowId,
    "number",
    "All requests have an innerWindowId attribute"
  );
  ok(
    actual.targetFront.isTargetFront,
    "All requests have a targetFront attribute"
  );

  for (const name in expected) {
    if (name == "targetFrontUrl") {
      is(
        actual.targetFront.url,
        expected[name],
        "The request matches the right target front"
      );
    } else {
      is(actual[name], expected[name], `The '${name}' attribute is correct`);
    }
  }
}

// Assert that the correct resource information are available for the resource update type
function assertNetworkUpdateResources(actual, expected) {
  const updateTypes = Object.keys(expected);
  const expectedUpdateType = updateTypes.find(
    type => actual[`${type}Available`]
  );
  const expectedUpdates = expected[expectedUpdateType];
  for (const name in expectedUpdates) {
    is(
      expectedUpdates[name],
      actual[name],
      `The resource update "${name}" contains the expected value "${actual[name]}"`
    );
  }
}
