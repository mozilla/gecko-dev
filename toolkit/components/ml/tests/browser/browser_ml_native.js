/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/// <reference path="head.js" />

/**
 * Runs a full end-to-end test on the native ONNX backend
 */
add_task(async function test_ml_smoke_test_onnx() {
  const { cleanup } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine({
    taskName: "text-classification",
    modelId: "acme/bert",
    dtype: "q8",
    backend: "onnx-native",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
  });
  const inferencePromise = engineInstance.run({ args: ["dummy data"] });

  const res = await inferencePromise;
  Assert.equal(res[0].label, "LABEL_0", "The text gets classified");

  await EngineProcess.destroyMLEngine();

  await cleanup();
});
