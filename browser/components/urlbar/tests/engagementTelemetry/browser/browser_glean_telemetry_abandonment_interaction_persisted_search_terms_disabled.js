/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test abandonment telemetry with persisted search terms disabled.

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_setup(async function () {
  await initInteractionTest();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", false]],
  });

  // Persisted Search requires app-provided engines.
  let cleanup = await installPersistTestEngines("MochiSearch");
  let engine = Services.search.getEngineByName("MochiSearch");
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.moveEngine(engine, 0);
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    cleanup();
  });
});

add_task(async function persisted_search_terms() {
  await doPersistedSearchTermsTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "typed" }]),
  });
});

add_task(async function persisted_search_terms_restarted_refined() {
  await doPersistedSearchTermsRestartedRefinedTest({
    enabled: false,
    trigger: () => doBlur(),
    assert: expected => assertAbandonmentTelemetry([{ interaction: expected }]),
  });
});

add_task(
  async function persisted_search_terms_restarted_refined_via_abandonment() {
    await doPersistedSearchTermsRestartedRefinedViaAbandonmentTest({
      enabled: false,
      trigger: () => doBlur(),
      assert: expected =>
        assertAbandonmentTelemetry([
          { interaction: "typed" },
          { interaction: expected },
        ]),
    });
  }
);
