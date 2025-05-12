/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { BackendError } = ChromeUtils.importESModule(
  "chrome://global/content/ml/backends/Pipeline.mjs"
);

add_task(async function test_backend_error() {
  const error = new BackendError("onnx", 12346);
  Assert.equal(error.name, "OnnxBackendError");
  Assert.equal(error.message, "Backend error: 12346");
});
