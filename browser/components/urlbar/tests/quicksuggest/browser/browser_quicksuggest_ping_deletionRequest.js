/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the `quick-suggest-deletion-request` Glean ping.

"use strict";

// The ping should be submitted when Suggest as a whole is disabled.
add_task(async function suggestDisabled() {
  await assertPingSubmitted(() => {
    UrlbarPrefs.set("quicksuggest.enabled", false);
  });

  UrlbarPrefs.clear("quicksuggest.enabled");
  await QuickSuggestTestUtils.forceSync();
});

// The ping should be submitted when AMP suggestions are disabled.
add_task(async function ampDisabled() {
  await assertPingSubmitted(() => {
    UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  });

  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  await QuickSuggestTestUtils.forceSync();
});

async function assertPingSubmitted(callback) {
  let submitted = false;
  GleanPings.quickSuggestDeletionRequest.testBeforeNextSubmit(() => {
    submitted = true;
  });

  await callback();
  await TestUtils.waitForCondition(
    () => submitted,
    "Waiting for testBeforeNextSubmit"
  );

  Assert.equal(
    Glean.quickSuggest.contextId.testGetValue(),
    expectedPingContextId(),
    "The ping should have contextId set"
  );
}
