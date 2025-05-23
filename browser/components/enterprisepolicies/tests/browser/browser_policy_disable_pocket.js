/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_POCKET = "extensions.pocket.enabled";

add_setup(async () => {
  // Bug 1968055 - Temporarily enabled pocket pref while we remove the pref entirely
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.enabled", true]],
  });
});

async function checkPocket(shouldBeEnabled) {
  return BrowserTestUtils.waitForCondition(() => {
    return (
      !!CustomizableUI.getWidget("save-to-pocket-button") == shouldBeEnabled
    );
  }, "Expecting Pocket to be " + shouldBeEnabled);
}

add_task(async function test_disable_firefox_screenshots() {
  await BrowserTestUtils.withNewTab("data:text/html,Test", async function () {
    // Sanity check to make sure Pocket is enabled on tests
    await checkPocket(true);

    await setupPolicyEngineWithJson({
      policies: {
        DisablePocket: true,
      },
    });

    await checkPocket(false);
  });
});
