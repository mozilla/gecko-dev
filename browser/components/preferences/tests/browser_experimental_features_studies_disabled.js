/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testHiddenWhenStudiesDisabled() {
  const cleanup = await setupLabsTest();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.preferences.experimental", true],
      ["browser.preferences.experimental.hidden", false],
    ],
  });

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#paneExperimental"
  );

  const doc = gBrowser.contentDocument;

  await waitForExperimentalFeaturesShown(doc);

  await enrollByClick(doc.getElementById("nimbus-qa-1"), true);
  await enrollByClick(doc.getElementById("nimbus-qa-2"), true);

  // Disabling studies should remove the experimental pane.
  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", false]],
  });
  await NimbusTestUtils.waitForActiveEnrollments([]);
  await waitForExperimentalFeaturesHidden(doc);

  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-1")?.active,
    "Should unenroll from nimbus-qa-1"
  );
  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-2")?.active,
    "Should unenroll from nimbus-qa-2"
  );

  // Re-enabling studies should re-add it.
  await SpecialPowers.popPrefEnv();
  await waitForExperimentalFeaturesShown(doc);

  // Navigate back to the experimental tab.
  EventUtils.synthesizeMouseAtCenter(
    doc.getElementById("category-experimental"),
    {},
    gBrowser.contentWindow
  );

  await waitForPageFlush();

  is(
    doc.querySelector(".category[selected]").id,
    "category-experimental",
    "Experimental category selected"
  );

  ok(
    !doc.getElementById("nimbus-qa-1").checked,
    "nimbus-qa-1 checkbox unchecked"
  );
  ok(
    !doc.getElementById("nimbus-qa-2").checked,
    "nimbus-qa-2 checkbox unchecked"
  );

  await enrollByClick(doc.getElementById("nimbus-qa-1"), true);
  await enrollByClick(doc.getElementById("nimbus-qa-2"), true);

  // Likewise, disabling telemetry should remove the experimental pane.
  await SpecialPowers.pushPrefEnv({
    set: [["datareporting.healthreport.uploadEnabled", false]],
  });

  await waitForExperimentalFeaturesHidden(doc);

  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-1")?.active,
    "Should unenroll from nimbus-qa-1"
  );
  ok(
    !ExperimentAPI._manager.store.get("nimbus-qa-2")?.active,
    "Should unenroll from nimbus-qa-2"
  );

  await SpecialPowers.popPrefEnv();

  // Re-enabling studies should re-add it.
  await waitForExperimentalFeaturesShown(doc);

  ok(
    !doc.getElementById("nimbus-qa-1").checked,
    "nimbus-qa-1 checkbox unchecked"
  );
  ok(
    !doc.getElementById("nimbus-qa-2").checked,
    "nimbus-qa-2 checkbox unchecked"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await cleanup();
  await SpecialPowers.popPrefEnv();
});

async function waitForExperimentalFeaturesShown(doc) {
  await TestUtils.waitForCondition(
    () => doc.querySelector(".featureGate"),
    "Wait for features to be added to the DOM"
  );

  ok(
    !doc.getElementById("category-experimental").hidden,
    "Experimental Features section should not be hidden"
  );

  ok(
    !Services.prefs.getBoolPref("browser.preferences.experimental.hidden"),
    "Hidden pref should be false"
  );
}

async function waitForExperimentalFeaturesHidden(doc) {
  await TestUtils.waitForCondition(
    () => doc.getElementById("category-experimental").hidden,
    "Wait for Experimental Features section to get hidden"
  );

  ok(
    doc.getElementById("category-experimental").hidden,
    "Experimental Features section should be hidden when all features are hidden"
  );
  ok(
    doc.getElementById("firefoxExperimentalCategory").hidden,
    "Experimental Features header should be hidden when all features are hidden"
  );
  is(
    doc.querySelector(".category[selected]").id,
    "category-general",
    "When the experimental features section is hidden, navigating to #experimental should redirect to #general"
  );
  ok(
    Services.prefs.getBoolPref("browser.preferences.experimental.hidden"),
    "Hidden pref should be true"
  );
}

function waitForPageFlush() {
  return new Promise(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(resolve))
  );
}
