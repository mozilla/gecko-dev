/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);

add_task(async function test_replace_body_in_designMode() {
  await BrowserTestUtils.withNewTab(
    "data:text/html,<html><body><br></body></html>",
    async function (browser) {
      const tipWrapper = new TIPWrapper(window);
      ok(
        tipWrapper.isAvailable(),
        "test_replace_body_in_designMode: TextInputProcessor should've been initialized"
      );

      function waitFor(aWaitingNotification) {
        return new Promise(resolve => {
          tipWrapper.onIMEFocusBlur = aNotification => {
            if (aNotification != aWaitingNotification) {
              return;
            }
            tipWrapper.onIMEFocusBlur = null;
            resolve();
          };
        });
      }
      const waitForInitialFocus = waitFor("notify-focus");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.designMode = "on";
        content.document.body.focus();
      });
      info("test_replace_body_in_designMode: Waiting for initial IME focus...");
      await waitForInitialFocus;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "test_replace_body_in_designMode: IME should be enabled when the document becomes editable"
      );

      const waitForRefocusAfterBodyRemoval = waitFor("notify-focus");
      await SpecialPowers.spawn(browser, [], () => {
        content.wrappedJSObject.body = content.document.body;
        content.document.body.remove();
      });
      info(
        "test_replace_body_in_designMode: Waiting for IME refocus after the <body> is removed..."
      );
      await waitForRefocusAfterBodyRemoval;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "test_replace_body_in_designMode: IME should be enabled after the <body> is removed"
      );

      const waitForRefocusAfterBodyReconnect = waitFor("notify-focus");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.documentElement.appendChild(
          content.wrappedJSObject.body
        );
      });
      info(
        "test_replace_body_in_designMode: Waiting for IME refocus after the <body> is reconnected..."
      );
      await waitForRefocusAfterBodyReconnect;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "test_replace_body_in_designMode: IME should be enabled after the <body> is reconnected"
      );
    }
  );
});

add_task(async function test_replace_document_element_in_designMode() {
  await BrowserTestUtils.withNewTab(
    "data:text/html,<html><body><br></body></html>",
    async function (browser) {
      const tipWrapper = new TIPWrapper(window);
      ok(
        tipWrapper.isAvailable(),
        "test_replace_document_element_in_designMode: TextInputProcessor should've been initialized"
      );

      function waitFor(aWaitingNotification) {
        return new Promise(resolve => {
          tipWrapper.onIMEFocusBlur = aNotification => {
            if (aNotification != aWaitingNotification) {
              return;
            }
            tipWrapper.onIMEFocusBlur = null;
            resolve();
          };
        });
      }
      const waitForInitialFocus = waitFor("notify-focus");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.designMode = "on";
        content.document.body.focus();
      });
      info(
        "test_replace_document_element_in_designMode: Waiting for initial IME focus..."
      );
      await waitForInitialFocus;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "test_replace_document_element_in_designMode: IME should be enabled when the document becomes editable"
      );

      tipWrapper.clearFocusBlurNotifications();
      const waitForBlurAfterDocumentElementRemoval = waitFor("notify-blur");
      await SpecialPowers.spawn(browser, [], () => {
        content.wrappedJSObject.documentElement =
          content.document.documentElement;
        content.document.documentElement.remove();
      });
      info(
        "test_replace_document_element_in_designMode: Waiting for IME blur after the <html> is removed..."
      );
      await waitForBlurAfterDocumentElementRemoval;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "test_replace_document_element_in_designMode: IME should be enabled after the <html> is removed"
      );

      info(
        "test_replace_document_element_in_designMode: Waiting for IME focus after the <html> is reconnected..."
      );
      const waitForRefocusAfterDocumentElementReconnect =
        waitFor("notify-focus");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.appendChild(content.wrappedJSObject.documentElement);
      });
      await waitForRefocusAfterDocumentElementReconnect;
      Assert.equal(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "test_replace_document_element_in_designMode: IME should be enabled after the <html> is reconnected"
      );
    }
  );
});
