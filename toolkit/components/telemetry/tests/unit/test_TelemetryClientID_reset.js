/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);
const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryStorage } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryStorage.sys.mjs"
);
const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);

const DELETION_REQUEST_PING_TYPE = "deletion-request";
const TEST_PING_TYPE = "test-ping-type";

function sendPing(addEnvironment = false) {
  let options = {
    addClientId: true,
    addEnvironment,
  };
  return TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, options);
}

add_task(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  await new Promise(resolve =>
    Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(resolve))
  );

  PingServer.start();
  Services.prefs.setStringPref(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
  await TelemetryController.testSetup();
});

/**
 * Testing the following scenario:
 *
 * 1. Telemetry upload gets disabled
 * 2. Canary client ID is set
 * 3. Instance is shut down
 * 4. Telemetry upload flag is toggled
 * 5. Instance is started again
 * 6. Detect that upload is enabled and reset client ID
 *
 * This scenario e.g. happens when switching between channels
 * with and without the deletion-request ping reset included.
 */
add_task(async function test_clientid_reset_after_reenabling() {
  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);
  Assert.ok("profileGroupId" in ping);

  let firstClientId = ping.clientId;
  let firstProfileGroupId = ping.profileGroupId;
  Assert.notEqual(
    TelemetryUtils.knownClientID,
    firstClientId,
    "Client ID should be valid and random"
  );

  // Disable FHR upload: this should trigger a deletion-request ping.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.FhrUploadEnabled,
    false
  );

  ping = await PingServer.promiseNextPing();
  Assert.equal(
    ping.type,
    DELETION_REQUEST_PING_TYPE,
    "The ping must be a deletion-request ping"
  );
  Assert.equal(ping.clientId, firstClientId);
  Assert.equal(ping.profileGroupId, firstProfileGroupId);
  let clientId = await ClientID.getClientID();
  Assert.equal(TelemetryUtils.knownClientID, clientId);
  let profileGroupId = await ClientID.getProfileGroupID();
  Assert.notEqual(
    firstProfileGroupId,
    profileGroupId,
    "The profile group ID should have been reset."
  );
  Assert.notEqual(
    profileGroupId,
    clientId,
    "The profile group ID should not match the new client ID."
  );

  // Now shutdown the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Flip the pref again
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Start the instance
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.notEqual(
    TelemetryUtils.knownClientID,
    newClientId,
    "Client ID should be valid and random"
  );
  Assert.notEqual(
    firstClientId,
    newClientId,
    "Client ID should be newly generated"
  );
  let newProfileGroupId = await ClientID.getProfileGroupID();
  Assert.notEqual(
    TelemetryUtils.knownProfileGroupID,
    newProfileGroupId,
    "The profile group ID should be valid and random"
  );
  Assert.notEqual(
    firstProfileGroupId,
    newProfileGroupId,
    "The profile group ID should have been reset."
  );
  Assert.notEqual(
    newProfileGroupId,
    newClientId,
    "The profile group ID should not match the client ID."
  );
});

/**
 * Testing the following scenario:
 * (Reverse of the first test)
 *
 * 1. Telemetry upload gets disabled, canary client ID is set
 * 2. Telemetry upload is enabled
 * 3. New client ID is generated.
 * 3. Instance is shut down
 * 4. Telemetry upload flag is toggled
 * 5. Instance is started again
 * 6. Detect that upload is disabled and sets canary client ID
 *
 * This scenario e.g. happens when switching between channels
 * with and without the deletion-request ping reset included.
 */
add_task(async function test_clientid_canary_after_disabling() {
  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);
  Assert.ok("profileGroupId" in ping);

  let firstClientId = ping.clientId;
  let firstProfileGroupId = ping.profileGroupId;
  Assert.notEqual(
    TelemetryUtils.knownClientID,
    firstClientId,
    "Client ID should be valid and random"
  );
  Assert.notEqual(
    TelemetryUtils.knownProfileGroupID,
    firstProfileGroupId,
    "Profile Group ID should be valid and random"
  );
  Assert.notEqual(
    firstClientId,
    firstProfileGroupId,
    "Profile Group ID should be valid and not match the client ID"
  );

  // Disable FHR upload: this should trigger a deletion-request ping.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.FhrUploadEnabled,
    false
  );

  ping = await PingServer.promiseNextPing();
  Assert.equal(
    ping.type,
    DELETION_REQUEST_PING_TYPE,
    "The ping must be a deletion-request ping"
  );
  Assert.equal(ping.clientId, firstClientId);
  Assert.equal(ping.profileGroupId, firstProfileGroupId);
  let clientId = await ClientID.getClientID();
  Assert.equal(TelemetryUtils.knownClientID, clientId);
  let profileGroupId = await ClientID.getProfileGroupID();
  Assert.equal(TelemetryUtils.knownProfileGroupID, profileGroupId);

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);
  await sendPing();
  ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.notEqual(
    firstClientId,
    ping.clientId,
    "Client ID should be newly generated"
  );
  Assert.notEqual(
    firstProfileGroupId,
    ping.profileGroupId,
    "Profile group ID should be newly generated"
  );
  Assert.notEqual(
    ping.profileGroupId,
    ping.clientId,
    "Profile group ID should not match the client ID"
  );

  // Now shutdown the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Flip the pref again
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.FhrUploadEnabled,
    false
  );

  // Start the instance
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.equal(
    TelemetryUtils.knownClientID,
    newClientId,
    "Client ID should be a canary when upload disabled"
  );
  let newProfileGroupId = await ClientID.getProfileGroupID();
  Assert.equal(
    TelemetryUtils.knownProfileGroupID,
    newProfileGroupId,
    "Profile group ID should be a canary when upload disabled"
  );
});

add_task(async function stopServer() {
  await PingServer.stop();
});
