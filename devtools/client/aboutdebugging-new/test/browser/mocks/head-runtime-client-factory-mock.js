/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../../shared/test/shared-head.js */

"use strict";

/**
 * Setup the loader to return the provided mock object instead of the regular
 * runtime-client-factory module.
 *
 * @param {Object}
 *        mock should implement the following methods:
 *        - createClientForRuntime(runtime)
 */
function enableRuntimeClientFactoryMock(mock) {
  const { setMockedModule } = require("devtools/client/aboutdebugging-new/src/modules/test-helper");
  setMockedModule(mock, "modules/runtime-client-factory");
}

/**
 * Update the loader to clear the mock entry for the runtime-client-factory module.
 */
function disableRuntimeClientFactoryMock() {
  const { removeMockedModule } = require("devtools/client/aboutdebugging-new/src/modules/test-helper");
  removeMockedModule("modules/runtime-client-factory");
}

/**
 * Creates a simple mock version for runtime-client-factory, implementing all the expected
 * methods with empty placeholders.
 */
function createRuntimeClientFactoryMock() {
  const RuntimeClientFactoryMock = {};
  RuntimeClientFactoryMock.createClientForRuntime = function(runtime) {
    console.log("MOCKED METHOD createClientForRuntime");
  };

  return RuntimeClientFactoryMock;
}
