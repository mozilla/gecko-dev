/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testNonPublicFeaturesShouldntGetDisplayed() {
  const cleanup = await setupLabsTest();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#paneExperimental"
  );
  let doc = gBrowser.contentDocument;

  await TestUtils.waitForCondition(
    () => doc.getElementById("nimbus-qa-1"),
    "wait for features to be added to the DOM"
  );

  Assert.ok(
    !!doc.getElementById("nimbus-qa-1"),
    "nimbus-qa-1 checkbox in the document"
  );
  Assert.ok(
    !!doc.getElementById("nimbus-qa-2"),
    "nimbus-qa-2 checkbox in the document"
  );

  Assert.ok(
    !doc.getElementById("targeting-false"),
    "targeting-false checkbox not in the document"
  );
  Assert.ok(
    !doc.getElementById("bucketing-false"),
    "bucketing-false checkbox not in the document"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await cleanup();
  await SpecialPowers.popPrefEnv();
});

add_task(async function testNonPublicFeaturesShouldntGetDisplayed() {
  // Only recipes that do not match targeting or bucketing
  const cleanup = await setupLabsTest(DEFAULT_LABS_RECIPES.slice(2));

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

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await cleanup();
  await SpecialPowers.popPrefEnv();
});
