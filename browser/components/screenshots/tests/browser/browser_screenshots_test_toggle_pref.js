/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

const COMPONENT_PREF = "screenshots.browser.component.enabled";
const SCREENSHOTS_PREF = "extensions.screenshots.disabled";

add_task(async function test_toggling_screenshots_pref() {
  let observerSpy = sinon.spy();
  let notifierSpy = sinon.spy();

  let observerStub = sinon
    .stub(ScreenshotsUtils, "observe")
    .callsFake(observerSpy);
  let notifierStub = sinon
    .stub(ScreenshotsUtils, "notify")
    .callsFake(function () {
      notifierSpy();
      ScreenshotsUtils.notify.wrappedMethod.apply(this, arguments);
    });

  // wait for startup idle tasks to complete
  await new Promise(resolve => ChromeUtils.idleDispatch(resolve));
  ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Component enabled");
  ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      await BrowserTestUtils.waitForCondition(
        () => ScreenshotsUtils.initialized,
        "The component is initialized"
      );
      ok(ScreenshotsUtils.initialized, "The component is initialized");

      ok(observerSpy.notCalled, "Observer not called");
      helper.triggerUIFromToolbar();
      Assert.equal(observerSpy.callCount, 1, "Observer function called once");

      ok(notifierSpy.notCalled, "Notifier not called");
      EventUtils.synthesizeKey("s", { accelKey: true, shiftKey: true });

      await TestUtils.waitForCondition(() => notifierSpy.callCount == 1);
      Assert.equal(notifierSpy.callCount, 1, "Notify function called once");

      await TestUtils.waitForCondition(() => observerSpy.callCount == 2);
      Assert.equal(observerSpy.callCount, 2, "Observer function called twice");

      let menu = document.getElementById("contentAreaContextMenu");
      let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.activateItem(menu.querySelector("#context-take-screenshot"));
      await popuphidden;

      Assert.equal(observerSpy.callCount, 3, "Observer function called thrice");

      let componentUnitialized = TestUtils.topicObserved(
        "screenshots-component-uninitialized"
      );

      Services.prefs.setBoolPref(COMPONENT_PREF, false);
      ok(
        !Services.prefs.getBoolPref(COMPONENT_PREF),
        "Component should be disabled"
      );

      info("Wait for the Screenshot component to be uninitialized");
      await componentUnitialized;
      ok(
        !ScreenshotsUtils.initialized,
        "Screenshot component should be uninitialized"
      );

      info("Triggering the screenshot again should be a no-op");
      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      info("Triggering the screenshot from the contextmenu should be a no-op");
      popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.activateItem(menu.querySelector("#context-take-screenshot"));
      await popuphidden;

      Assert.equal(
        observerSpy.callCount,
        3,
        "Observer function still called thrice"
      );

      let componentReady = TestUtils.topicObserved(
        "screenshots-component-initialized"
      );

      info("Re-enabling the Screenshot component should re-initialize it");

      Services.prefs.setBoolPref(COMPONENT_PREF, true);
      ok(Services.prefs.getBoolPref(COMPONENT_PREF), "Component enabled");
      // Needed for component to initialize
      await componentReady;

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        4,
        "Observer function called four times"
      );
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let componentUnitialized = TestUtils.topicObserved(
        "screenshots-component-uninitialized"
      );

      Services.prefs.setBoolPref(SCREENSHOTS_PREF, true);
      Services.prefs.setBoolPref(COMPONENT_PREF, true);

      ok(Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots disabled");

      info("Wait for the screenshot component to be uninitialized");
      await componentUnitialized;

      ok(
        document.getElementById("screenshot-button").disabled,
        "Toolbar button disabled"
      );

      let menu = document.getElementById("contentAreaContextMenu");
      let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
      EventUtils.synthesizeMouseAtCenter(document.body, {
        type: "contextmenu",
      });
      await popupshown;
      Assert.equal(menu.state, "open", "Context menu is open");

      ok(
        menu.querySelector("#context-take-screenshot").hidden,
        "Take screenshot is not in context menu"
      );

      let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
      menu.hidePopup();
      await popuphidden;

      let componentReady = TestUtils.topicObserved(
        "screenshots-component-initialized"
      );

      Services.prefs.setBoolPref(SCREENSHOTS_PREF, false);

      ok(!Services.prefs.getBoolPref(SCREENSHOTS_PREF), "Screenshots enabled");

      await componentReady;

      ok(ScreenshotsUtils.initialized, "The component is initialized");

      ok(
        !document.getElementById("screenshot-button").disabled,
        "Toolbar button is enabled"
      );

      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      Assert.equal(
        observerSpy.callCount,
        5,
        "Observer function called for the fifth time"
      );
    }
  );

  observerStub.restore();
  notifierStub.restore();
});
