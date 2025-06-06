/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - available_semantic_sources

let canUseSemanticStub;

add_setup(async function () {
  await initInteractionTest();

  const { getPlacesSemanticHistoryManager } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs"
  );
  let semanticManager = getPlacesSemanticHistoryManager();
  canUseSemanticStub = sinon.stub(semanticManager, "canUseSemanticSearch");
  registerCleanupFunction(() => {
    canUseSemanticStub.restore();
  });
});

add_task(async function test_semantic() {
  for (let canUseSemantic of [true, false]) {
    canUseSemanticStub.get(() => canUseSemantic);

    await doTypedTest({
      trigger: () => doEnter(),
      assert: () =>
        assertEngagementTelemetry([
          {
            available_semantic_sources: canUseSemantic ? "history" : "none",
          },
        ]),
    });

    await doTypedWithResultsPopupTest({
      trigger: () => doEnter(),
      assert: () =>
        assertEngagementTelemetry([
          {
            available_semantic_sources: canUseSemantic ? "history" : "none",
          },
        ]),
    });
  }
});
