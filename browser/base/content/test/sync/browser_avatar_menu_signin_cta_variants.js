/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

/**
 * Tests that we can change the CTA for the FxA sign-in button in the avatar
 * menu using Nimbus.
 */

const DEFAULT_HEADER = gSync.fluentStrings.formatValueSync(
  "synced-tabs-fxa-sign-in"
);
const DEFAULT_DESCRIPTION = gSync.fluentStrings.formatValueSync(
  "fxa-menu-sync-description"
);

add_setup(async () => {
  gSync.init();
});

/**
 * Closes and re-opens the avatar menu for the current browser window, and
 * returns a Promise once the main view fires the "ViewShown" event. The Promise
 * resolves to the object with properties pointing at the header and the
 * description text of the sign-in button.
 *
 * If the panel is not already open, this just opens it.
 *
 * @returns {Promise<object>}
 */
async function reopenAvatarMenu() {
  let widgetPanel = document.getElementById("customizationui-widget-panel");
  // The customizationui-widget-panel is created lazily, and destroyed upon
  // closing, meaning that if we didn't find it, it's not open.
  if (widgetPanel) {
    let panelHidden = BrowserTestUtils.waitForEvent(widgetPanel, "popuphidden");
    widgetPanel.hidePopup();
    await panelHidden;
  }
  let promiseViewShown = BrowserTestUtils.waitForEvent(
    PanelMultiView.getViewNode(document, "PanelUI-fxa"),
    "ViewShown"
  );
  await gSync.toggleAccountPanel(
    document.getElementById("fxa-toolbar-menu-button"),
    new MouseEvent("mousedown")
  );
  await promiseViewShown;
  let headerEl = PanelMultiView.getViewNode(document, "fxa-menu-header-title");
  let descriptionEl = PanelMultiView.getViewNode(
    document,
    "fxa-menu-header-description"
  );

  return {
    header: headerEl.value,
    description: descriptionEl.value,
  };
}

/**
 * Tests that we use the default CTA when not enrolled in an experiment.
 */
add_task(async function test_default() {
  Assert.equal(
    NimbusFeatures.fxaAvatarMenuItem.getVariable("ctaCopyVariant"),
    undefined,
    "Should not start with a NimbusFeature set for the CTA copy."
  );
  let sandbox = sinon.createSandbox();
  sandbox.spy(NimbusFeatures.fxaAvatarMenuItem, "recordExposureEvent");
  let { header, description } = await reopenAvatarMenu();
  Assert.equal(header, DEFAULT_HEADER, "Should have the default header.");
  Assert.equal(
    description,
    DEFAULT_DESCRIPTION,
    "Should have the default description."
  );
  Assert.ok(
    NimbusFeatures.fxaAvatarMenuItem.recordExposureEvent.notCalled,
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
      featureId: NimbusFeatures.fxaAvatarMenuItem.featureId,
      value: {
        ctaCopyVariant: "control",
      },
    },
    { isRollout: true }
  );
  let sandbox = sinon.createSandbox();
  sandbox.spy(NimbusFeatures.fxaAvatarMenuItem, "recordExposureEvent");
  let { header, description } = await reopenAvatarMenu();
  Assert.equal(header, DEFAULT_HEADER, "Should have the default header.");
  Assert.equal(
    description,
    DEFAULT_DESCRIPTION,
    "Should have the default description."
  );
  Assert.ok(
    NimbusFeatures.fxaAvatarMenuItem.recordExposureEvent.calledOnce,
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
    sandbox.spy(NimbusFeatures.fxaAvatarMenuItem, "recordExposureEvent");

    let expectedHeader = gSync.fluentStrings.formatValueSync(
      `fxa-menu-message-${variant}-primary-text`
    );
    let expectedDescription = gSync.fluentStrings.formatValueSync(
      `fxa-menu-message-${variant}-secondary-text`
    );
    let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
      {
        featureId: NimbusFeatures.fxaAvatarMenuItem.featureId,
        value: {
          ctaCopyVariant: variant,
        },
      },
      { isRollout: true }
    );

    let { header, description } = await reopenAvatarMenu();
    Assert.equal(header, expectedHeader, "Should have the expected header.");
    Assert.equal(
      description,
      expectedDescription,
      "Should have the expected description."
    );
    await doCleanup();

    ({ header, description } = await reopenAvatarMenu());
    Assert.equal(header, DEFAULT_HEADER, "Should have the default header.");
    Assert.equal(
      description,
      DEFAULT_DESCRIPTION,
      "Should have the default description."
    );
    Assert.ok(
      NimbusFeatures.fxaAvatarMenuItem.recordExposureEvent.calledOnce,
      "Recorded exposure."
    );
    sandbox.restore();
  }
});
