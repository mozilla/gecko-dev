/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/head.js",
  this
);

const { MLAutofill } = ChromeUtils.importESModule(
  "resource://autofill/MLAutofill.sys.mjs"
);

async function setup() {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", true],
      ["browser.ml.logLevel", "All"],
      ["browser.ml.modelCacheTimeout", 1000],
    ],
  });

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

add_setup(async function () {
  const { cleanup, remoteClients } = await setup();

  sinon.stub(MLAutofill, "run").callsFake(request => {
    const context = request.args[0];
    /* we need the input has id attribute and the value is the field name.
     * For example, <input id="cc-name" autocomplete="cc-name">
     */
    const match = context.match(/id="([^"]*)"/);
    return [
      {
        label: match[1],
        score: 0.9999,
      },
    ];
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.ml.experiment.enabled", true],
      ["extensions.formautofill.ml.experiment.runInAutomation", true],
    ],
  });

  await clearGleanTelemetry();

  registerCleanupFunction(async function () {
    MLAutofill.shutdown();
    await remoteClients["ml-onnx-runtime"].rejectPendingDownloads(1);
    await EngineProcess.destroyMLEngine();
    await cleanup();

    sinon.restore();
    await clearGleanTelemetry();
  });
});

add_task(async function test_run_ml_experiment() {
  function buildExpected(fieldName) {
    return {
      infer_field_name: fieldName,
      infer_reason: "autocomplete",
      fathom_infer_label: "",
      fathom_infer_score: "",
      ml_revision: "",
      ml_infer_label: fieldName,
      ml_infer_score: "0.9999",
    };
  }

  const expected_events = [
    buildExpected("cc-name"),
    buildExpected("cc-number"),
    buildExpected("cc-exp-month"),
    buildExpected("cc-exp-year"),
    buildExpected("cc-csc"),
    buildExpected("cc-type"),
  ];

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      const focusInput = "#cc-number";
      await SimpleTest.promiseFocus(browser);
      await focusAndWaitForFieldsIdentified(browser, focusInput);

      await Services.fog.testFlushAllChildren();
    }
  );

  let actual_events =
    Glean.formautofillMl.fieldInferResult.testGetValue() ?? [];
  Assert.equal(
    actual_events.length,
    expected_events.length,
    `Expected to have ${expected_events.length} events`
  );

  for (let i = 0; i < actual_events.length; i++) {
    Assert.deepEqual(actual_events[i].extra, expected_events[i]);
  }
});
