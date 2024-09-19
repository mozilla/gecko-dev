/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation buttons' state with RFPTarget::CSSPrefersColorScheme enabled
const TEST_URI = URL_ROOT_SSL + "doc_media_queries.html";

async function runTest(enabled) {
  const sign = enabled ? "+" : "-";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      [
        "privacy.fingerprintingProtection.overrides",
        `${sign}CSSPrefersColorScheme`,
      ],
    ],
  });

  await addTab(TEST_URI);
  const { inspector } = await openRuleView();

  info("Check that the color scheme simulation buttons exist");
  const lightButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-light-toggle"
  );
  const darkButton = inspector.panelDoc.querySelector(
    "#color-scheme-simulation-dark-toggle"
  );
  ok(lightButton, "The light color scheme simulation button exists");
  ok(darkButton, "The dark color scheme simulation button exists");

  const expectedState = enabled ? "disabled" : "enabled";
  is(lightButton.disabled, enabled, `Light button is ${expectedState}`);
  is(darkButton.disabled, enabled, `Dark button is ${expectedState}`);

  await SpecialPowers.popPrefEnv();
}

add_task(async function () {
  await runTest(true);
});

add_task(async function () {
  await runTest(false);
});
