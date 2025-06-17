/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { _ExperimentFeature: ExperimentFeature, ExperimentAPI } =
  ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

NimbusTestUtils.init(this);

add_setup(async function () {
  await ExperimentAPI.ready();

  const sandbox = sinon.createSandbox();

  /* We stub the functions that operate with enrollments and remote rollouts
   * so that any access to store something is implicitly validated against
   * the schema and no records have missing (or extra) properties while in tests
   */

  const origAddExperiment = ExperimentAPI.manager.store.addEnrollment.bind(
    ExperimentAPI.manager.store
  );
  sandbox
    .stub(ExperimentAPI.manager.store, "addEnrollment")
    .callsFake((enrollment, recipe) => {
      NimbusTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment, recipe);
    });

  // Ensure the inner callback runs after all other registered cleanup
  // functions. This lets tests use registerCleanupFunction to clean up any
  // stray enrollments.
  registerCleanupFunction(() => {
    registerCleanupFunction(async () => {
      await NimbusTestUtils.assert.storeIsEmpty(ExperimentAPI.manager.store);
      sandbox.restore();
    });
  });
});

async function setupTest() {
  await ExperimentAPI.ready();
  await ExperimentAPI._rsLoader.finishedUpdating();

  await ExperimentAPI._rsLoader.remoteSettingsClients.experiments.db.importChanges(
    {},
    Date.now(),
    [],
    { clear: true }
  );

  await ExperimentAPI._rsLoader.updateRecipes("test");

  return async function cleanup() {
    await NimbusTestUtils.removeStore(ExperimentAPI.manager.store);
  };
}

/**
 * Wait for a message from a child process.
 *
 * @param {string} msg
 * The message to wait for.
 *
 * @returns {Promise<void>}
 * A promise that resolves when the message is received for the first time.
 */
function waitForChildMessage(msg) {
  return new Promise(resolve => {
    const listener = () => {
      resolve();
      info(`parent received ${msg}`);
      Services.ppmm.removeMessageListener(msg, listener);
    };

    info(`parent waiting for ${msg}`);
    Services.ppmm.addMessageListener(msg, listener);
  });
}

/**
 * Set up a listener for a SharedData update in the process corresponding to the
 * specified browser.
 *
 * You must await the promise returned by this function *before* triggering a
 * SharedData flush.
 *
 * After triggering the flush, you must await the promise inside the returned
 * object.
 *
 * Example:
 *
 * ```js
 * const childUpdated = await childSharedDataChanged(browser);
 * // Do something to modify SharedData
 * Services.ppmm.sharedData.flush();
 * await childUpdated.promise;
 * ```
 *
 * @returns {Promise<object>}
 * A promise that resolves to an object containing a promise. The outer promise
 * resolves when the event handler has been registered in the child. The inner
 * promise resolves when the event has fired in the child.
 */
async function childSharedDataChanged(browser) {
  const MESSAGE = "nimbus-browser-test:shared-data-changed";

  const promise = waitForChildMessage(MESSAGE);
  await SpecialPowers.spawn(browser, [MESSAGE], async MESSAGE => {
    Services.cpmm.sharedData.addEventListener(
      "change",
      async () => {
        await Services.cpmm.sendAsyncMessage(MESSAGE);
      },
      { once: true }
    );
  });

  // We can't return promise here because JavaScript will collapse it and
  // awaiting this function will await *that* promise, which we don't want to
  // do.
  return { promise };
}
