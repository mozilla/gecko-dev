/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test for the nsIHttpChannelInternal helper setResponseOverride.
// which allows to bypass the network for a request before connect, and
// reply with a mocked response instead.
add_task(async function test_set_response_override() {
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic) {
      aSubject = aSubject.QueryInterface(Ci.nsIHttpChannelInternal);
      if (
        aTopic == "http-on-before-connect" &&
        aSubject.URI.spec ==
          "https://example.com/browser/netwerk/test/browser/dummy.html"
      ) {
        const replacedHttpResponse = Cc[
          "@mozilla.org/network/replaced-http-response;1"
        ].createInstance(Ci.nsIReplacedHttpResponse);
        replacedHttpResponse.responseStatus = 200;
        replacedHttpResponse.responseStatusText = "Och Aye";
        replacedHttpResponse.responseBody =
          "<div id=from-response-override>From setResponseOverride";
        replacedHttpResponse.setResponseHeader(
          "some-header",
          "some-value",
          false
        );
        replacedHttpResponse.setResponseHeader(
          "Set-Cookie",
          "foo=bar;Path=/",
          false
        );
        aSubject.setResponseOverride(replacedHttpResponse);
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-before-connect");

  const onTabLoaded = BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/browser/netwerk/test/browser/dummy.html",
      waitForLoad: true,
    },
    async function (browser) {
      await ContentTask.spawn(browser, [], async function () {
        Assert.ok(
          !!content.document.getElementById("from-response-override"),
          "Page was loaded using the response override"
        );
        Assert.equal(
          content.document.cookie,
          "foo=bar",
          "Cookie was set from the response override headers"
        );

        // Perform another request to the same URL to check status and headers override.
        const response = await content.fetch(
          "https://example.com/browser/netwerk/test/browser/dummy.html"
        );
        Assert.equal(
          response.status,
          200,
          "Status was set from the response override"
        );
        Assert.equal(
          response.statusText,
          "Och Aye",
          "Status text was set from the response override"
        );
        Assert.equal(
          response.headers.get("some-header"),
          "some-value",
          "same-header header was set from the response override"
        );
      });
    }
  );
  await onTabLoaded;

  Services.obs.removeObserver(observer, "http-on-before-connect");
});

// Test that a response override with 302 Found status + Location header
// redirects to the URL specified in the Location header.
add_task(async function test_set_response_override_redirects() {
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic) {
      aSubject = aSubject.QueryInterface(Ci.nsIHttpChannelInternal);
      if (
        aTopic == "http-on-before-connect" &&
        aSubject.URI.spec ==
          "https://example.com/browser/netwerk/test/browser/dummy.html"
      ) {
        const replacedHttpResponse = Cc[
          "@mozilla.org/network/replaced-http-response;1"
        ].createInstance(Ci.nsIReplacedHttpResponse);
        replacedHttpResponse.responseStatus = 302;
        replacedHttpResponse.responseStatusText = "Found";
        replacedHttpResponse.setResponseHeader(
          "Location",
          "https://example.com/browser/netwerk/test/browser/dummy.html?redirected=true",
          false
        );
        aSubject.setResponseOverride(replacedHttpResponse);
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-before-connect");

  const onTabLoaded = BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/browser/netwerk/test/browser/dummy.html",
      waitForLoad: true,
    },
    async function (browser) {
      await ContentTask.spawn(browser, [], async function () {
        Assert.equal(
          content.location.href,
          "https://example.com/browser/netwerk/test/browser/dummy.html?redirected=true",
          "Navigation was redirected based on the overridden response"
        );
      });
    }
  );
  await onTabLoaded;

  Services.obs.removeObserver(observer, "http-on-before-connect");
});
