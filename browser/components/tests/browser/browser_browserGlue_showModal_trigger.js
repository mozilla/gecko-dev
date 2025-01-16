/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

const TEST_SCREEN = [
  {
    id: "TEST_SCREEN",
    content: {
      position: "split",
      logo: {},
      title: "test",
    },
  },
];

const TEST_SCREEN_SELECTOR = `.onboardingContainer .screen.${TEST_SCREEN[0].id}`;

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

async function showAboutWelcomeModal(
  screens = "",
  modalScreens = "",
  requireAction = false
) {
  const PREFS_TO_SET = [
    ["browser.startup.homepage_override.mstone", ""],
    ["startup.homepage_welcome_url", "about:welcome"],
    ["browser.aboutwelcome.modalScreens", modalScreens],
    ["browser.aboutwelcome.screens", screens],
    ["browser.aboutwelcome.requireAction", requireAction],
    ["browser.aboutwelcome.showModal", true],
  ];
  await SpecialPowers.pushPrefEnv({
    set: PREFS_TO_SET,
  });

  BrowserHandler.firstRunProfile = true;
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  registerCleanupFunction(async () => {
    PREFS_TO_SET.forEach(pref => Services.prefs.clearUserPref(pref[0]));
    BrowserHandler.firstRunProfile = false;
  });
}

add_task(async function show_about_welcome_modal() {
  let messageSpy = sinon.spy(SpecialMessageActions, "handleAction");
  await showAboutWelcomeModal(JSON.stringify(TEST_SCREEN));
  const [win] = await TestUtils.topicObserved("subdialog-loaded");

  Assert.notEqual(
    Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .getFirstWindowArgs(),
    "about:welcome",
    "First window will not be about:welcome"
  );

  // Wait for screen content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(TEST_SCREEN_SELECTOR)
  );

  Assert.equal(
    messageSpy.firstCall.args[0].data.content.disableEscClose,
    false
  );

  Assert.ok(
    !!win.document.querySelector(TEST_SCREEN_SELECTOR),
    "Modal renders with custom about:welcome screen"
  );

  await win.close();
  sinon.restore();
});

add_task(async function shows_modal_with_custom_screens_over_about_welcome() {
  let messageSpy = sinon.spy(SpecialMessageActions, "handleAction");
  await showAboutWelcomeModal("", JSON.stringify(TEST_SCREEN), true);
  const [win] = await TestUtils.topicObserved("subdialog-loaded");

  Assert.equal(
    Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .getFirstWindowArgs(),
    "about:welcome",
    "First window will be about:welcome"
  );

  // Wait for screen content to render
  await TestUtils.waitForCondition(() =>
    win.document.querySelector(TEST_SCREEN_SELECTOR)
  );

  Assert.equal(messageSpy.firstCall.args[0].data.content.disableEscClose, true);

  Assert.ok(
    !!win.document.querySelector(TEST_SCREEN_SELECTOR),
    "Modal renders with custom modal screen"
  );

  await win.close();
  sinon.restore();
});
