/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests ensure that the right values are sent for the client_association
 * metrics, depending on the FxA signed-in state. Note that this does NOT test
 * the "ride along" mechanism that the fxAccounts ping uses to be sent at the
 * same frequency as the baseline ping, as this is something that is implemented
 * and tested externally, in Glean.
 */

"use strict";

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);

const FAKE_UID = "0123456789abcdef0123456789abcdef";
let gClientID = null;

add_setup(async () => {
  gClientID = await ClientID.getClientID();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["identity.fxaccounts.telemetry.clientAssociationPing.enabled", true],
    ],
  });
});

/**
 * A helper function that mocks out the accounts UIState to be in a particular
 * status, and then forces the fxAccounts ping to be sent before running a
 * taskFn to see what the values that'd be sent are.
 *
 * @param {string} status
 *   One of the UIState.STATUS_* constants.
 * @param {function} taskFn
 *   A synchronous function that will be called just before the fxAccounts ping
 *   is sent.
 */
async function testMockUIState(status, taskFn) {
  let sandbox = sinon.createSandbox();
  sandbox.stub(UIState, "get").returns({
    status,
    lastSync: new Date(),
    email: "test@example.com",
    uid: FAKE_UID,
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  try {
    let checkValues = new Promise(resolve => {
      GleanPings.fxAccounts.testBeforeNextSubmit(() => {
        taskFn();
        resolve();
      });
    });
    GleanPings.fxAccounts.submit();
    await checkValues;
  } finally {
    sandbox.restore();
  }
}

/**
 * Tests that the accounts uid and legacy telemetry client ID are sent if
 * the state is STATUS_SIGNED_IN.
 */
add_task(async function test_client_association_logged_in() {
  await testMockUIState(UIState.STATUS_SIGNED_IN, async () => {
    Assert.equal(
      Glean.clientAssociation.uid.testGetValue(),
      FAKE_UID,
      "Got expected account uid"
    );
    Assert.equal(
      Glean.clientAssociation.legacyClientId.testGetValue(),
      gClientID,
      "Got expected legacy telemetry client ID"
    );
  });
});

/**
 * Tests that the accounts uid and legacy telemetry client ID are NOT sent if
 * the state is not STATUS_SIGNED_IN.
 */
add_task(async function test_client_association_not_logged_in() {
  for (let status of [
    UIState.STATUS_NOT_CONFIGURED,
    UIState.STATUS_LOGIN_FAILED,
    UIState.STATUS_NOT_VERIFIED,
  ]) {
    await testMockUIState(status, async () => {
      Assert.equal(
        Glean.clientAssociation.uid.testGetValue(),
        null,
        "No value set for account uid"
      );
      Assert.equal(
        Glean.clientAssociation.legacyClientId.testGetValue(),
        null,
        "No value set for legacy telemetry client ID"
      );
    });
  }
});
