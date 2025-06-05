"use strict";

const { OnboardingMessageProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs"
);

const { BookmarksBarButton } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/BookmarksBarButton.sys.mjs"
);

const { ASRouterTargeting } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTargeting.sys.mjs"
);

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);

const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

const sandbox = sinon.createSandbox();
let originalLocale = Services.locale;

const buttonMessage = {
  id: "FINISH_SETUP_BUTTON",
  groups: [],
  template: "bookmarks_bar_button",
  skip_in_tests: "fails unrelated tests",
  content: {
    label: {
      raw: "Finish setup",
    },
    logo: {
      imageURL: "chrome://branding/content/about-logo.png",
    },
    action: {
      type: "SET_PREF",
      data: {
        pref: {
          name: "easyChecklist.open",
          value: true,
        },
      },
    },
  },
  priority: 1,
  trigger: {
    id: "defaultBrowserCheck",
  },
  targeting:
    "((currentDate|date - profileAgeCreated|date) / 86400000 <= 7) && localeLanguageCode == 'en'",
};

function restoreOriginalLocale() {
  Services.locale = originalLocale;
}

function setNewLocale(locale) {
  Services.locale = new Proxy(originalLocale, {
    get(target, prop) {
      if (prop === "appLocaleAsBCP47") {
        return locale;
      }

      if (prop === "appLocaleAsLangTag") {
        return locale;
      }
      return target[prop];
    },
  });
}

async function getSetupButton(win) {
  await TestUtils.waitForCondition(
    () => win.document.getElementById("fxms-bmb-button"),
    "Waiting for setup button to appear"
  );

  return win.document.getElementById("fxms-bmb-button");
}

function cleanup() {
  restoreOriginalLocale();
  Services.prefs.clearUserPref("messaging-system-action.easyChecklist.open");
  Services.prefs.clearUserPref("browser.toolbars.bookmarks.visibility");
  sandbox.restore();
}

// Tests that the button is added to the bookmarks toolbar, and that clicking it triggers its action
add_task(async function test_finish_setup_button() {
  setNewLocale("en-US");
  const setPrefStub = sandbox.stub(SpecialMessageActions, "handleAction");
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser.ownerGlobal,
    "about:newtab"
  );

  await BookmarksBarButton.showBookmarksBarButton(
    tab.linkedBrowser.ownerGlobal,
    buttonMessage
  );

  const win = tab.ownerGlobal;
  const button = await getSetupButton(win);

  // Test button exists
  Assert.ok(button, "Finish setup button should be created");

  const image = button.querySelector("image");
  Assert.ok(image, "Image exists on button");

  const label = button.querySelector("image");
  Assert.ok(label, "Label exists on button");

  Assert.equal(
    button.getAttribute("label"),
    "Finish setup",
    "Button should have correct label attribute"
  );

  button.click();

  let setPrefCalled = setPrefStub.calledWith({
    type: "SET_PREF",
    data: {
      pref: {
        name: "easyChecklist.open",
        value: true,
      },
    },
  });
  // Verify the action was called
  Assert.ok(
    setPrefCalled,
    "Button click should call SET_PREF action to open checklist"
  );

  await BrowserTestUtils.removeTab(tab);
  cleanup();
});

// Tests that the feature callout opens when the pref is changed to true
add_task(async function test_feature_callout_open_on_pref() {
  setNewLocale("en-US");
  const impressionSpy = sandbox.spy(ASRouter, "addImpression");
  const messageId = "FINISH_SETUP_CHECKLIST";

  await BookmarksBarButton.showBookmarksBarButton(
    gBrowser.ownerGlobal,
    buttonMessage
  );

  const selectors = [
    ".action-checklist-subtitle",
    ".action-checklist-progress-bar",
    "#action-checklist-set-to-default",
    "#action-checklist-pin-to-taskbar",
    "#action-checklist-import-data",
    "#action-checklist-explore-extensions",
    "#action-checklist-sign-in",
  ];

  const win = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url: "about:newtab",
  });

  // Set checklist pref open for checklist targeting
  Services.prefs.setBoolPref(
    "messaging-system-action.easyChecklist.open",
    true
  );

  await TestUtils.waitForCondition(
    () => win.document.querySelector(`.${messageId}`),
    "Waiting for feature_callout to auto-open"
  );
  const callout = win.document.querySelector(`.${messageId}`);
  Assert.ok(callout, "Feature callout should open when pref is true");

  for (let selector of selectors) {
    Assert.ok(
      win.document.querySelector(selector),
      `Element present with selector ${selector}`
    );
  }

  Assert.ok(
    impressionSpy.calledTwice,
    "Message impression applied for button and callout"
  );

  callout.querySelector(".dismiss-button").click();
  await TestUtils.waitForCondition(
    () => !win.document.querySelector(`.${messageId}`),
    "Waiting for feature_callout to be dismissed"
  );

  await BrowserTestUtils.closeWindow(win);
  cleanup();
});
