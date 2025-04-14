/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { ExperimentFakes, ExperimentTestUtils, NimbusTestUtils } =
  ChromeUtils.importESModule(
    "resource://testing-common/NimbusTestUtils.sys.mjs"
  );

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
});

NimbusTestUtils.init(this);

add_setup(function () {
  let sandbox = sinon.createSandbox();

  /* We stub the functions that operate with enrollments and remote rollouts
   * so that any access to store something is implicitly validated against
   * the schema and no records have missing (or extra) properties while in tests
   */

  let origAddExperiment = ExperimentManager.store.addEnrollment.bind(
    ExperimentManager.store
  );
  sandbox
    .stub(ExperimentManager.store, "addEnrollment")
    .callsFake(enrollment => {
      ExperimentTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment);
    });

  registerCleanupFunction(() => {
    sandbox.restore();
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
    await NimbusTestUtils.removeStore(ExperimentAPI._manager.store);
  };
}

async function assertEmptyStore(store) {
  await NimbusTestUtils.removeStore(store);
}
