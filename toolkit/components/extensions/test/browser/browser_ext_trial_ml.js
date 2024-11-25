/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

/* import-globals-from ../../../ml/tests/browser/head.js */
loadTestSubscript("../../../ml/tests/browser/head.js");

async function happyPath() {
  const options = {
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  };

  await browser.trial.ml.createEngine(options);

  const data = ["This gets echoed."];

  browser.test.sendMessage("model_created");
  const inferencePromise = browser.trial.ml.runEngine({
    args: data,
  });

  browser.test.sendMessage("promise_created");

  const res = (await inferencePromise).output;

  // The `test-echo` task does not load a real model but
  // creates a fully functional worker in the infefence engine that returns
  // the same values abd the options it received.
  browser.test.assertDeepEq(
    res,
    data,
    "The text get echoed exercising the whole flow."
  );
  browser.test.sendMessage("inference_finished");
}

async function disabledFeature() {
  const options = {
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  };

  try {
    await browser.trial.ml.createEngine(options);
    throw Error("Should fail");
  } catch (err) {}

  browser.test.sendMessage("model_created");
  browser.test.sendMessage("promise_created");
  browser.test.sendMessage("inference_finished");
}

function createMlExtensionTest({
  testName,
  backgroundFunction = happyPath,
  prefs = [
    ["extensions.experiments.enabled", true],
    ["extensions.ml.enabled", true],
  ],
}) {
  const func = async function () {
    await SpecialPowers.pushPrefEnv({
      set: prefs,
    });

    const { cleanup, remoteClients } = await setup();
    const id = Services.uuid.generateUUID().number;
    ExtensionPermissions.add(id, { permissions: ["trialML"], origins: [] });

    let extension = ExtensionTestUtils.loadExtension({
      isPrivileged: true,
      manifest: {
        optional_permissions: ["trialML"],
        background: { scripts: ["background.js"] },
        browser_specific_settings: { gecko: { id } },
      },
      files: {
        "background.js": backgroundFunction,
      },
    });

    await extension.startup();
    try {
      await extension.awaitMessage("model_created");
      await extension.awaitMessage("promise_created");
      await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
      await extension.awaitMessage("inference_finished");
    } finally {
      await extension.unload();
      await EngineProcess.destroyMLEngine();
      await cleanup();
      await SpecialPowers.popPrefEnv();
    }
  };

  Object.defineProperty(func, "name", { value: testName });
  return func;
}

/**
 * Testing that the API won't work if the preferences are not set
 */
add_task(
  createMlExtensionTest({
    testName: "no_pref",
    backgroundFunction: disabledFeature,
    prefs: [
      ["extensions.experiments.enabled", true],
      ["extensions.ml.enabled", false],
    ],
  })
);

/**
 * Testing the happy path.
 */
add_task(createMlExtensionTest({ testName: "happy_path" }));

/**
 * Testing errors when options are not valid
 */
add_task(
  createMlExtensionTest({
    testName: "options_error",
    backgroundFunction: async function backgroundScript() {
      const options = {
        taskName: "summari@#zation",
        modelId: "test-echo",
        modelRevision: "main",
      };

      try {
        await browser.trial.ml.createEngine(options);
        browser.test.fail("Bad options should be caught");
      } catch (err) {
        browser.test.assertTrue(
          err.message.startsWith("Unsupported task summari@#zation")
        );
      }

      browser.test.sendMessage("model_created");
      browser.test.sendMessage("promise_created");
      browser.test.sendMessage("inference_finished");
    },
  })
);

add_task(
  createMlExtensionTest({
    testName: "options_error_2",
    backgroundFunction: async function backgroundScript() {
      const options = {
        taskName: "summarization",
        modelId: "test-ec@ho",
        modelRevision: "main",
      };

      try {
        await browser.trial.ml.createEngine(options);
        browser.test.fail("Bad options should be caught");
      } catch (err) {
        browser.test.assertTrue(err.message.startsWith("Invalid value"));
      }

      browser.test.sendMessage("model_created");
      browser.test.sendMessage("promise_created");
      browser.test.sendMessage("inference_finished");
    },
  })
);
