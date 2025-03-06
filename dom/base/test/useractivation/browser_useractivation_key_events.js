/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);
const TEST_URL = BASE + "empty.html";

async function synthesizeKeyAndTest(aBrowser, aKey, aEvent, aIsActive) {
  let promise = SpecialPowers.spawn(
    aBrowser,
    [aKey, aEvent, aIsActive],
    async (key, event, isActive) => {
      return new Promise(aResolve => {
        content.document.clearUserGestureActivation();
        content.document.addEventListener(
          "keydown",
          function (e) {
            e.preventDefault();
            is(
              content.document.hasBeenUserGestureActivated,
              isActive,
              `check has-been-user-activated for ${key} with ${JSON.stringify(event)}`
            );
            is(
              content.document.hasValidTransientUserGestureActivation,
              isActive,
              `check has-valid-transient-user-activation for ${key} with ${JSON.stringify(event)}`
            );
            aResolve();
          },
          { once: true }
        );
      });
    }
  );
  // Ensure the event listener has registered on the remote.
  await SpecialPowers.spawn(aBrowser, [], () => {
    return new Promise(resolve => {
      SpecialPowers.executeSoon(resolve);
    });
  });
  EventUtils.synthesizeKey(aKey, aEvent);
  return promise;
}

let browser;
add_setup(async function setup() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  browser = tab.linkedBrowser;
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function TestPrintableKey() {
  let tests = ["a", "b", "c", "A", "B", "1", "2", "3"];

  for (let key of tests) {
    await synthesizeKeyAndTest(browser, key, {}, true);
  }
});

add_task(async function TestNonPrintableKey() {
  let tests = [
    ["KEY_Backspace", false],
    ["KEY_Control", false],
    ["KEY_Shift", false],
    ["KEY_Escape", false],
    // Treat as user input
    ["KEY_Tab", true],
    ["KEY_Enter", true],
    [" ", true],
  ];

  for (let [key, expectedResult] of tests) {
    await synthesizeKeyAndTest(browser, key, {}, expectedResult);
  }
});

add_task(async function TestModifier() {
  let tests = [
    ["a", { accelKey: true }, false],
    ["z", { accelKey: true }, false],
    ["a", { metaKey: true }, !navigator.platform.includes("Mac")],
    // Treat as user input
    ["a", { altGraphKey: true }, true],
    ["a", { fnKey: true }, true],
    ["a", { altKey: true }, true],
    ["a", { shiftKey: true }, true],
    ["c", { altKey: true }, true],
    ["c", { accelKey: true }, true],
    ["v", { altKey: true }, true],
    ["v", { accelKey: true }, true],
    ["x", { altKey: true }, true],
    ["x", { accelKey: true }, true],
  ];

  for (let [key, event, expectedResult] of tests) {
    await synthesizeKeyAndTest(browser, key, event, expectedResult);
  }
});
