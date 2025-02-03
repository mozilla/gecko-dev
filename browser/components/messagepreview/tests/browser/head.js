"use strict";

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  ContentTaskUtils: "resource://testing-common/ContentTaskUtils.sys.mjs",
});

async function getAboutMessagePreviewParent(browser) {
  let windowGlobalParent = browser.browsingContext.currentWindowGlobal;
  return windowGlobalParent.getActor("AboutMessagePreview");
}

async function waitForClick(selector, win) {
  let el = await TestUtils.waitForCondition(() =>
    win.document.querySelector(selector)
  );
  el.click();
}

async function dialogClosed(browser) {
  await TestUtils.waitForCondition(
    () => !browser?.ownerGlobal.gDialogBox.isOpen
  );
}

function selectorIsVisible(selector) {
  const els = document.querySelectorAll(selector);
  // The offsetParent will be null if element is hidden through "display: none;"
  return [...els].some(el => el.offsetParent !== null);
}

function clearNotifications() {
  for (let notification of PopupNotifications._currentNotifications) {
    notification.remove();
  }

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
}

async function openMessagePreviewTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:messagepreview",
    true
  );

  return {
    browser: tab.linkedBrowser,
    cleanup: async () => {
      BrowserTestUtils.removeTab(tab);
    },
  };
}

/**
 * Function to test message UI
 *
 * @param {object} win Browser window
 * @param {string} experiment Message output by the test
 * @param {string} message_id ID of the test message
 * @param {Array} selectors HTML elements to check for
 */
async function test_window_message_content(
  win,
  experiment,
  message_id,
  selectors = []
) {
  // Wait for main content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(`main.${message_id}`)
  );

  for (let selector of selectors) {
    Assert.ok(
      win.document.querySelector(selector),
      `Element present with selector ${selector}`
    );
  }
}

async function test_private_message_content(win, experiment, selectors = []) {
  await SpecialPowers.spawn(
    win,
    [experiment, selectors],
    // eslint-disable-next-line no-shadow
    async function (experiment, selectors) {
      // Wait for main content to render
      await ContentTaskUtils.waitForCondition(() =>
        content.document.documentElement.hasAttribute(
          "PrivateBrowsingRenderComplete"
        )
      );

      for (let selector of selectors) {
        Assert.ok(
          content.document.documentElement.querySelector(selector),
          `Element present with selector ${selector}`
        );
      }
    }
  );
}
