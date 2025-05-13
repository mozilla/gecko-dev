/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

// Tests that swipe-to-navigation can be triggered on documents where the root
// scroll container can not be scrolled horizontally even if the scroll
// container is the current target of the active wheel transaction.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      // success-velocity-contribution is very high and pixel-size is
      // very low so that one swipe goes over the threshold asap.
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 999999.0],
      ["widget.swipe.pixel-size", 1.0],
      ["dom.event.wheel-event-groups.enabled", true],
      ["mousewheel.transaction.timeout", 10000],
    ],
  });

  // Load two pages in order.
  // NOTE: the second page is vertically scrollable, is not horizontally
  // scrollable.
  const firstPage = "about:about";
  const secondPage = "data:text/html,<html style='height:500vh'></html>";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);

  // Setup an active wheel event listener in the second page so that APZ needs to
  // wait for the response from the content whether the event was consumed or not.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.window.addEventListener("wheel", () => {}, { passive: false });
  });

  // Setup a scrollend event listener to tell whether scrolling has ended.
  const scrollendPromise = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(resolve => {
      content.window.addEventListener(
        "scrollend",
        () => {
          resolve();
        },
        { once: true }
      );
    });
  });

  // Flush the above spawned task queue.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(resolve => resolve());
  });

  // Scroll down the content by pan gestures.
  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    0,
    NativePanHandler.delta,
    NativePanHandler.beginPhase
  );
  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    0,
    0,
    NativePanHandler.endPhase
  );

  // Make sure the scroll has ended.
  await scrollendPromise;

  const loadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    firstPage
  );

  // Now trigger a swipe navigation to the previous page.
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);

  // If the swipe navigation was triggered, the previous page should be loaded.
  await loadPromise;

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
