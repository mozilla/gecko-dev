"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const kPage =
  "https://example.org/browser/dom/base/test/fullscreen/dummy_page.html";
const kInterval = 3000;

add_task(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["dom.fullscreen.force_exit_on_multiple_escape_interval", kInterval]
  );

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: kPage,
    waitForStateStop: true,
  });
  let browser = tab.linkedBrowser;

  // As requestFullscreen checks the active state of the docshell,
  // wait for the document to be activated, just to be sure that
  // the fullscreen request won't be denied.
  await SpecialPowers.spawn(browser, [], () => {
    // Setup for form validate.
    let div = content.document.createElement("div");
    div.innerHTML = `
      <form>
        <input required>
        <button type="submit">Submit</button>
      </form>
    `;
    content.document.body.appendChild(div);

    return ContentTaskUtils.waitForCondition(
      () => content.browsingContext.isActive && content.document.hasFocus()
    );
  });

  let state;
  info("Enter DOM fullscreen");
  let fullScreenChangedPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "fullscreenchange"
  );
  await SpecialPowers.spawn(browser, [], () => {
    content.document.body.requestFullscreen();
  });

  await fullScreenChangedPromise;
  state = await SpecialPowers.spawn(browser, [], () => {
    return !!content.document.fullscreenElement;
  });
  ok(state, "The content should have entered fullscreen");
  ok(document.fullscreenElement, "The chrome should also be in fullscreen");

  info("Open form validation popup (1)");
  let popupShownPromise = promiseWaitForEvent(window, "popupshown");
  await SpecialPowers.spawn(browser, [], () => {
    content.document.querySelector("button").click();
  });
  await popupShownPromise;
  let invalidFormPopup = document.getElementById("invalid-form-popup");
  is(invalidFormPopup.state, "open", "Should have form validation popup");
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Synthesize Esc key (1)");
  let popupHidePromise = promiseWaitForEvent(window, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidePromise;
  is(
    invalidFormPopup.state,
    "closed",
    "Should have closed form validation popup"
  );
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Wait for multiple-escape-handling interval to expire");
  await new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, kInterval + 100);
  });

  info("Open form validation popup (2)");
  popupShownPromise = promiseWaitForEvent(window, "popupshown");
  await SpecialPowers.spawn(browser, [], () => {
    content.document.querySelector("button").click();
  });
  await popupShownPromise;
  is(invalidFormPopup.state, "open", "Should have form validation popup");
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Synthesize Esc key (2)");
  popupHidePromise = promiseWaitForEvent(window, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidePromise;
  is(
    invalidFormPopup.state,
    "closed",
    "Should have closed form validation popup"
  );
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Open form validation popup (3)");
  popupShownPromise = promiseWaitForEvent(window, "popupshown");
  await SpecialPowers.spawn(browser, [], () => {
    content.document.querySelector("button").click();
  });
  await popupShownPromise;
  is(invalidFormPopup.state, "open", "Should have form validation popup");
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Synthesize Esc key (3)");
  let fullscreenExitPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "fullscreenchange"
  );
  popupHidePromise = promiseWaitForEvent(window, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidePromise;
  is(
    invalidFormPopup.state,
    "closed",
    "Should have closed form validation popup"
  );
  await fullscreenExitPromise;
  ok(!document.fullscreenElement, "The chrome should have exited fullscreen");

  gBrowser.removeTab(tab);
});
