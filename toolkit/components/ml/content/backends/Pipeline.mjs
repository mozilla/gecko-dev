/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(
  lazy,
  {
    ONNXPipeline: "chrome://global/content/ml/backends/ONNXPipeline.mjs",
    LlamaPipeline: "chrome://global/content/ml/backends/LlamaPipeline.mjs",
    PipelineOptions: "chrome://global/content/ml/EngineProcess.sys.mjs",
  },
  { global: "current" }
);

/**
 * Initializes and returns the appropriate backend pipeline based on the given options.
 *
 * @param {object} consumer - The worker object that interacts with the backend.
 * @param {ArrayBuffer} wasm - The WebAssembly module required for execution.
 * @param {object} options - Configuration options for the pipeline.
 *
 * @returns {Promise<object>} A promise that resolves to the initialized pipeline.
 */
export async function getBackend(consumer, wasm, options) {
  const pipelineOptions = new lazy.PipelineOptions(options);
  var factory;

  switch (pipelineOptions.backend) {
    case "onnx":
      factory = lazy.ONNXPipeline.initialize;
      break;
    case "wllama":
      factory = lazy.LlamaPipeline.initialize;
      break;
    default:
      factory = lazy.ONNXPipeline.initialize;
  }

  return await factory(consumer, wasm, pipelineOptions);
}
