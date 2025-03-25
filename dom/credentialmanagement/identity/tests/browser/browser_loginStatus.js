/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdentityCredentialStorageService",
  "@mozilla.org/browser/identity-credential-storage-service;1",
  "nsIIdentityCredentialStorageService"
);

const TEST_URL = "https://example.com/";
const TEST_XORIGIN_URL = "https://example.net/";
const TEST_LOGIN_STATUS_BASE =
  TEST_URL +
  "browser/dom/credentialmanagement/identity/tests/browser/server_loginStatus.sjs?status=";
const TEST_LOGIN_STATUS_XORIGIN_BASE =
  TEST_XORIGIN_URL +
  "browser/dom/credentialmanagement/identity/tests/browser/server_loginStatus.sjs?status=";

/**
 * Perform a test with a function that should change a page's login status.
 *
 * This function opens a new foreground tab, then calls stepFn with two arguments:
 * the Browser of the new tab and the value that should be set as the login status.
 * This repeats for various values of the header, making sure the status is correct
 * after each instance.
 *
 * @param {Function(Browser, string) => Promise} stepFn - The function to update login status,
 * @param {string} - An optional description describing the test case, used in assertion descriptions.
 */
async function login_logout_sequence(stepFn, desc = "") {
  // Open a test page, get its principal
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  const principal = gBrowser.contentPrincipal;

  // Make sure we don't have a starting permission
  let permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.UNKNOWN_ACTION,
    "Permission correctly not initialized in test of " + desc
  );

  // Try using a bad value for the argument
  await stepFn(tab.linkedBrowser, "should-reject");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.UNKNOWN_ACTION,
    "Permission not altered by bad enum value in test of " + desc
  );

  // Try logging in
  await stepFn(tab.linkedBrowser, "logged-in");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.ALLOW_ACTION,
    "Permission stored correcty for `logged-in` in test of " + desc
  );

  // Try logging out
  await stepFn(tab.linkedBrowser, "logged-out");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.DENY_ACTION,
    "Permission stored correcty for `logged-out` in test of " + desc
  );

  // Try using a bad value for the argument, after it's already been set
  await stepFn(tab.linkedBrowser, "should-reject");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.DENY_ACTION,
    "Permission not altered by bad enum value in test of " + desc
  );

  // Try logging in again
  await stepFn(tab.linkedBrowser, "logged-in");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.ALLOW_ACTION,
    "Permission stored correcty for `logged-in` in test of " + desc
  );

  // Make sure we don't have any extra permissinons laying about
  let permissions = Services.perms.getAllByTypes(["self-reported-logged-in"]);
  Assert.equal(
    permissions.length,
    1,
    "One permission must be left after all modifications in test of " + desc
  );

  // Clear the permission
  Services.perms.removeByType("self-reported-logged-in");

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
}

/**
 * Perform a test with a function that should NOT change a page's login status.
 *
 * This function opens a new foreground tab, then calls stepFn with two arguments:
 * the Browser of the new tab and the value that should be set as the login status.
 * Then it makes sure that no permission has been set for the login status.
 *
 * @param {Function(Browser, string) => Promise} stepFn - The function to update login status,
 * @param {string} - An optional description describing the test case, used in assertion descriptions.
 */
async function login_doesnt_work(stepFn, desc = "") {
  // Open a test page, get its principal
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  const principal = gBrowser.contentPrincipal;

  // Make sure we don't have a starting permission
  let permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.UNKNOWN_ACTION,
    "Permission correctly not initialized in test of " + desc
  );

  // Try logging in
  await stepFn(tab.linkedBrowser, "logged-in");
  permission = Services.perms.testPermissionFromPrincipal(
    principal,
    "self-reported-logged-in"
  );
  Assert.equal(
    permission,
    Services.perms.UNKNOWN_ACTION,
    "Permission not set for `logged-in` in test of " + desc
  );

  // Make sure we don't have any extra permissinons laying about
  let permissions = Services.perms.getAllByTypes(["self-reported-logged-in"]);
  Assert.equal(permissions.length, 0, "No permission set in test of " + desc);

  // Clear the permission
  Services.perms.removeByType("self-reported-logged-in");

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
}

// Function that we can use to set the status and return with a string for the different possible outcomes
async function setStatusInContent(status) {
  try {
    let result = await content.navigator.login.setStatus(status);
    if (result === undefined) {
      return "resolved with undefined";
    }
    return "resolved with defined";
  } catch (err) {
    if (err.name == "TypeError") {
      return "rejected with TypeError";
    }
    if (err.name == "SecurityError") {
      return "rejected with SecurityError";
    }
    return "rejected with other error";
  }
}

add_task(async function test_logiStatus_js() {
  let setLoginStatusInJavascript = async function (browser, value) {
    let loginResult = await SpecialPowers.spawn(
      browser,
      [value],
      setStatusInContent
    );
    if (value == "logged-in" || value == "logged-out") {
      Assert.equal(
        "resolved with undefined",
        loginResult,
        "Successful call resolves with `undefined`"
      );
    } else {
      Assert.equal(
        loginResult,
        "rejected with TypeError",
        "Unsuccessful JS call rejects with TypeError"
      );
    }
  };

  await login_logout_sequence(setLoginStatusInJavascript, "javascript API");
});

add_task(async function test_loginStatus_js_frame() {
  let setLoginStatusInSubframeJavascript = async function (browser, value) {
    const iframeBC = await SpecialPowers.spawn(
      browser,
      [TEST_URL],
      async url => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = url;
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      }
    );
    let loginResult = await SpecialPowers.spawn(
      iframeBC,
      [value],
      setStatusInContent
    );
    if (value == "logged-in" || value == "logged-out") {
      Assert.equal(
        "resolved with undefined",
        loginResult,
        "Successful call resolves with `undefined`"
      );
    } else {
      Assert.equal(
        loginResult,
        "rejected with TypeError",
        "Unsuccessful JS call rejects with TypeError"
      );
    }
  };

  await login_logout_sequence(
    setLoginStatusInSubframeJavascript,
    "javascript API"
  );
});

add_task(async function test_loginStatus_js_xorigin_frame() {
  let setLoginStatusInSubframeJavascript = async function (browser, value) {
    const iframeBC = await SpecialPowers.spawn(
      browser,
      [TEST_XORIGIN_URL],
      async url => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = url;
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      }
    );
    let loginResult = await SpecialPowers.spawn(
      iframeBC,
      [value],
      setStatusInContent
    );
    if (value == "logged-in" || value == "logged-out") {
      Assert.equal(
        loginResult,
        "rejected with SecurityError",
        "Cross origin JS call with correct enum rejects with SecurityError"
      );
    } else {
      Assert.equal(
        loginResult,
        "rejected with TypeError",
        "Unsuccessful JS call rejects with TypeError"
      );
    }
  };

  await login_doesnt_work(setLoginStatusInSubframeJavascript, "javascript API");
});

add_task(async function test_loginStatus_js_xorigin_ancestor_frame() {
  let setLoginStatusInSubframeJavascript = async function (browser, value) {
    const iframeBC = await SpecialPowers.spawn(
      browser,
      [TEST_XORIGIN_URL],
      async url => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = url;
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      }
    );
    const innerIframeBC = await SpecialPowers.spawn(
      iframeBC,
      [TEST_URL],
      async url => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = url;
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      }
    );
    let loginResult = await SpecialPowers.spawn(
      innerIframeBC,
      [value],
      setStatusInContent
    );
    if (value == "logged-in" || value == "logged-out") {
      Assert.equal(
        loginResult,
        "rejected with SecurityError",
        "Cross origin JS call with correct enum rejects with SecurityError"
      );
    } else {
      Assert.equal(
        loginResult,
        "rejected with TypeError",
        "Unsuccessful JS call rejects with TypeError"
      );
    }
  };

  await login_doesnt_work(setLoginStatusInSubframeJavascript, "javascript API");
});

add_task(async function test_login_logout_document_headers() {
  let setLoginStatusInDocumentHeader = async function (browser, value) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, TEST_URL);
    BrowserTestUtils.startLoadingURIString(
      browser,
      TEST_LOGIN_STATUS_BASE + value
    );
    await loaded;
  };

  await login_logout_sequence(
    setLoginStatusInDocumentHeader,
    "document redirect"
  );
});

add_task(async function test_loginStatus_subresource_headers() {
  let setLoginStatusViaHeader = async function (browser, value) {
    await SpecialPowers.spawn(
      browser,
      [TEST_LOGIN_STATUS_BASE + value],
      async function (url) {
        await content.fetch(url);
      }
    );
  };

  await login_logout_sequence(setLoginStatusViaHeader, "subresource header");
});

add_task(async function test_loginStatus_xorigin_subresource_headers() {
  let setLoginStatusViaHeader = async function (browser, value) {
    await SpecialPowers.spawn(
      browser,
      [TEST_LOGIN_STATUS_XORIGIN_BASE + value],
      async function (url) {
        await content.fetch(url, { mode: "no-cors" });
      }
    );
  };
  await login_doesnt_work(
    setLoginStatusViaHeader,
    "xorigin subresource header"
  );
});

add_task(
  async function test_loginStatus_xorigin_subdocument_subresource_headers() {
    let setLoginStatusViaSubdocumentSubresource = async function (
      browser,
      value
    ) {
      const iframeBC = await SpecialPowers.spawn(
        browser,
        [TEST_XORIGIN_URL],
        async url => {
          const iframe = content.document.createElement("iframe");
          await new Promise(resolve => {
            iframe.addEventListener("load", resolve, { once: true });
            iframe.src = url;
            content.document.body.appendChild(iframe);
          });

          return iframe.browsingContext;
        }
      );
      await SpecialPowers.spawn(
        iframeBC,
        [TEST_LOGIN_STATUS_BASE + value],
        async function (url) {
          await content.fetch(url, { mode: "no-cors" });
        }
      );
    };
    await login_doesnt_work(
      setLoginStatusViaSubdocumentSubresource,
      "xorigin subresource header in xorigin frame"
    );
  }
);

add_task(
  async function test_loginStatus_xorigin_subdocument_xorigin_subresource_headers() {
    let setLoginStatusViaSubdocumentSubresource = async function (
      browser,
      value
    ) {
      const iframeBC = await SpecialPowers.spawn(
        browser,
        [TEST_XORIGIN_URL],
        async url => {
          const iframe = content.document.createElement("iframe");
          await new Promise(resolve => {
            iframe.addEventListener("load", resolve, { once: true });
            iframe.src = url;
            content.document.body.appendChild(iframe);
          });

          return iframe.browsingContext;
        }
      );
      await SpecialPowers.spawn(
        iframeBC,
        [TEST_LOGIN_STATUS_XORIGIN_BASE + value],
        async function (url) {
          await content.fetch(url, { mode: "no-cors" });
        }
      );
    };
    await login_doesnt_work(
      setLoginStatusViaSubdocumentSubresource,
      "xorigin subresource header in xorigin frame"
    );
  }
);

add_task(async function test_loginStatus_subdocument_subresource_headers() {
  let setLoginStatusViaSubdocumentSubresource = async function (
    browser,
    value
  ) {
    const iframeBC = await SpecialPowers.spawn(
      browser,
      [TEST_URL],
      async url => {
        const iframe = content.document.createElement("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = url;
          content.document.body.appendChild(iframe);
        });

        return iframe.browsingContext;
      }
    );
    await SpecialPowers.spawn(
      iframeBC,
      [TEST_LOGIN_STATUS_BASE + value],
      async function (url) {
        await content.fetch(url, { mode: "no-cors" });
      }
    );
  };
  await login_logout_sequence(
    setLoginStatusViaSubdocumentSubresource,
    "subresource header in frame"
  );
});
