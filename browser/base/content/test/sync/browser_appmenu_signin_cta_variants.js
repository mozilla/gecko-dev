/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

/**
 * Tests that we can change the CTA for the FxA sign-in button in the AppMenu
 * using Nimbus.
 */

add_setup(function () {
  registerCleanupFunction(() => {
    PanelUI.hide();
  });
});

/**
 * Closes and re-opens the AppMenu for the current browser window, and returns a
 * Promise once the main view fires the "ViewShown" event. The Promise resolves
 * to the element in the AppMenu that holds the FxA sign-in CTA.
 *
 * If the panel is not already open, this just opens it.
 *
 * @returns {Promise<Element>}
 */
async function reopenAppMenu() {
  PanelUI.hide();
  let promiseViewShown = BrowserTestUtils.waitForEvent(
    PanelUI.panel,
    "ViewShown"
  );
  PanelUI.show();
  await promiseViewShown;
  return PanelMultiView.getViewNode(document, "appMenu-fxa-text");
}

/**
 * Tests that we use the default CTA when not enrolled in an experiment.
 */
add_task(async function test_default() {
  let sandbox = sinon.createSandbox();
  sandbox.spy(NimbusFeatures.fxaAppMenuItem, "recordExposureEvent");
  Assert.equal(
    NimbusFeatures.fxaAppMenuItem.getVariable("ctaCopyVariant"),
    undefined,
    "Should not start with a NimbusFeature set for the CTA copy."
  );
  let ctaEl = await reopenAppMenu();
  Assert.equal(
    ctaEl.dataset.l10nId,
    "appmenu-fxa-sync-and-save-data2",
    "Should have the default CTA text."
  );
  Assert.ok(
    NimbusFeatures.fxaAppMenuItem.recordExposureEvent.notCalled,
    "Did not record any exposure."
  );
  sandbox.restore();
});

/**
 * Tests that the control variant uses the default CTA, but also records an
 * impression.
 */
add_task(async function test_control() {
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.fxaAppMenuItem.featureId,
      value: {
        ctaCopyVariant: "control",
      },
    },
    { isRollout: true }
  );
  let sandbox = sinon.createSandbox();
  sandbox.spy(NimbusFeatures.fxaAppMenuItem, "recordExposureEvent");
  let ctaEl = await reopenAppMenu();
  Assert.equal(
    ctaEl.dataset.l10nId,
    "appmenu-fxa-sync-and-save-data2",
    "Should have the default CTA text."
  );

  Assert.ok(
    NimbusFeatures.fxaAppMenuItem.recordExposureEvent.calledOnce,
    "Recorded exposure."
  );
  await doCleanup();
  sandbox.restore();
});

/**
 * Tests all variants , and that when we stop experimenting with that variant,
 * we revert back to the default CTA.
 */
add_task(async function test_variants() {
  let variants = ["sync-devices", "backup-data", "backup-sync", "mobile"];

  for (let variant of variants) {
    let sandbox = sinon.createSandbox();
    sandbox.spy(NimbusFeatures.fxaAppMenuItem, "recordExposureEvent");

    let expectedL10nID = `fxa-menu-message-${variant}-collapsed-text`;
    // Ensure that a string actually exists for that ID.
    try {
      Assert.ok(
        await document.l10n.formatValue(expectedL10nID),
        `Found a string for ${expectedL10nID}`
      );
    } catch (e) {
      Assert.ok(false, `Missing string for ID: ${expectedL10nID}`);
    }
    let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
      {
        featureId: NimbusFeatures.fxaAppMenuItem.featureId,
        value: {
          ctaCopyVariant: variant,
        },
      },
      { isRollout: true }
    );

    let ctaEl = await reopenAppMenu();
    Assert.equal(
      ctaEl.dataset.l10nId,
      expectedL10nID,
      "Got the expected string for the variant."
    );

    await doCleanup();

    ctaEl = await reopenAppMenu();
    Assert.equal(
      ctaEl.dataset.l10nId,
      "appmenu-fxa-sync-and-save-data2",
      "Should have the default CTA text."
    );
    Assert.ok(
      NimbusFeatures.fxaAppMenuItem.recordExposureEvent.calledOnce,
      "Recorded exposure."
    );
    sandbox.restore();
  }
});
