/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FirefoxLabs } = ChromeUtils.importESModule(
  "resource://nimbus/FirefoxLabs.sys.mjs"
);

add_setup(async function setup() {
  const cleanup = await setupLabsTest();
  registerCleanupFunction(cleanup);
});

// This test verifies that pressing the reset all button for experimental features
// resets all of the checkboxes to their default state.
add_task(async function testResetAll() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences#paneExperimental`
  );

  const doc = gBrowser.contentDocument;

  await TestUtils.waitForCondition(
    () => doc.querySelector(".featureGate"),
    "wait for features to be added to the DOM"
  );

  const qa1El = doc.getElementById("nimbus-qa-1");
  const qa2El = doc.getElementById("nimbus-qa-2");

  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-1")?.active,
    "Should not enroll in nimbus-qa-1"
  );
  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-2")?.active,
    "Should not enroll in nimbus-qa-2"
  );
  ok(!qa1El.checked, "nimbus-qa-1 checkbox unchecked");
  ok(!qa2El.checked, "nimbus-qa-2 checkbox unchecked");

  // Modify the state of some of the features.
  await enrollByClick(qa1El, true);
  await enrollByClick(qa2El, true);

  ok(
    ExperimentAPI._manager.store.get("nimbus-qa-1")?.active,
    "Should enroll in nimbus-qa-1"
  );
  ok(
    ExperimentAPI._manager.store.get("nimbus-qa-2")?.active,
    "Should enroll in nimbus-qa-2"
  );
  ok(qa1El.checked, "nimbus-qa-1 checkbox checked");
  ok(qa2El.checked, "nimbus-qa-2 checkbox checked");

  const unenrollPromises = [
    promiseNimbusStoreUpdate("nimbus-qa-1", false),
    promiseNimbusStoreUpdate("nimbus-qa-2", false),
  ];

  doc.getElementById("experimentalCategory-reset").click();
  await Promise.all(unenrollPromises);

  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-1")?.active,
    "Should unenroll from nimbus-qa-1"
  );
  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-2")?.active,
    "Should unenroll from nimbus-qa-2"
  );
  ok(!qa1El.checked, "nimbus-qa-1 checkbox unchecked");
  ok(!qa2El.checked, "nimbus-qa-2 checkbox unchecked");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
