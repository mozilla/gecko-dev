/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";
const PREF_CACHED_PROFILEGROUPID = "toolkit.telemetry.cachedProfileGroupID";

var drsPath;

const uuidRegex =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

function run_test() {
  do_get_profile();
  drsPath = PathUtils.join(PathUtils.profileDir, "datareporting", "state.json");

  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );
  run_next_test();
}

add_task(function test_setup() {
  // FOG needs a profile and to be init.
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_client_id() {
  const invalidIDs = [
    [-1, "setIntPref"],
    [0.5, "setIntPref"],
    ["INVALID-UUID", "setStringPref"],
    [true, "setBoolPref"],
    ["", "setStringPref"],
    ["3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ", "setStringPref"],
  ];

  // If there is no DRS file, and no cached id, we should get a new client ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let clientID = await ClientID.getClientID();
  Assert.equal(typeof clientID, "string");
  Assert.ok(uuidRegex.test(clientID));
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  await IOUtils.writeUTF8(drsPath, "abcd", {
    tmpPath: drsPath + ".tmp",
  });
  clientID = await ClientID.getClientID();
  Assert.equal(typeof clientID, "string");
  Assert.ok(uuidRegex.test(clientID));
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // If the DRS data is broken, we should end up with the cached ID.
  let oldClientID = clientID;
  for (let [invalidID] of invalidIDs) {
    await ClientID._reset();
    await IOUtils.writeJSON(drsPath, { clientID: invalidID });
    clientID = await ClientID.getClientID();
    Assert.equal(clientID, oldClientID);
    if (AppConstants.platform != "android") {
      Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
    }
  }

  // Test that valid DRS actually works.
  const validClientID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  await ClientID._reset();
  await IOUtils.writeJSON(drsPath, { clientID: validClientID });
  clientID = await ClientID.getClientID();
  Assert.equal(clientID, validClientID);
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // Test that reloading a valid DRS works.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  clientID = await ClientID.getClientID();
  Assert.equal(clientID, validClientID);
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // Assure that cached IDs are being checked for validity.
  for (let [invalidID, prefFunc] of invalidIDs) {
    await ClientID._reset();
    Services.prefs[prefFunc](PREF_CACHED_CLIENTID, invalidID);
    let cachedID = ClientID.getCachedClientID();
    Assert.strictEqual(
      cachedID,
      null,
      "ClientID should ignore invalid cached IDs"
    );
    Assert.ok(
      !Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID),
      "ClientID should reset invalid cached IDs"
    );
    Assert.ok(
      Services.prefs.getPrefType(PREF_CACHED_CLIENTID) ==
        Ci.nsIPrefBranch.PREF_INVALID,
      "ClientID should reset invalid cached IDs"
    );
  }
});

add_task(async function test_profile_group_id() {
  const invalidIDs = [
    [-1, "setIntPref"],
    [0.5, "setIntPref"],
    ["INVALID-UUID", "setStringPref"],
    [true, "setBoolPref"],
    ["", "setStringPref"],
    ["3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ", "setStringPref"],
  ];

  // If there is no DRS file, and no cached id, we should get a new profile group ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(typeof profileGroupID, "string");
  Assert.ok(uuidRegex.test(profileGroupID));
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.writeUTF8(drsPath, "abcd", {
    tmpPath: drsPath + ".tmp",
  });
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(typeof profileGroupID, "string");
  Assert.ok(uuidRegex.test(profileGroupID));
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // If the DRS data is broken, we should end up with the cached ID.
  let oldGroupID = profileGroupID;
  for (let [invalidID] of invalidIDs) {
    await ClientID._reset();
    await IOUtils.writeJSON(drsPath, { clientID: invalidID });
    profileGroupID = await ClientID.getProfileGroupID();
    Assert.equal(profileGroupID, oldGroupID);
    if (AppConstants.platform != "android") {
      Assert.equal(
        profileGroupID,
        Glean.legacyTelemetry.profileGroupId.testGetValue()
      );
    }
  }

  // Test that valid DRS actually works.
  const validProfileGroupID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  const validClientID = "d06361a2-67d8-4d41-b804-6fab6ddf5461";
  await ClientID._reset();
  await IOUtils.writeJSON(drsPath, {
    clientID: validClientID,
    profileGroupID: validProfileGroupID,
  });
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(profileGroupID, validProfileGroupID);
  let clientID = await ClientID.getClientID();
  Assert.equal(clientID, validClientID);
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // Test that valid DRS actually works when the client ID is missing.
  await ClientID._reset();
  await IOUtils.writeJSON(drsPath, {
    profileGroupID: validProfileGroupID,
  });
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(profileGroupID, validProfileGroupID);
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // Test that reloading a valid DRS works.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(profileGroupID, validProfileGroupID);
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // Assure that cached IDs are being checked for validity.
  for (let [invalidID, prefFunc] of invalidIDs) {
    await ClientID._reset();
    Services.prefs[prefFunc](PREF_CACHED_PROFILEGROUPID, invalidID);
    let cachedID = ClientID.getCachedProfileGroupID();
    Assert.strictEqual(
      cachedID,
      null,
      "ClientID should ignore invalid cached IDs"
    );
    Assert.ok(
      !Services.prefs.prefHasUserValue(PREF_CACHED_PROFILEGROUPID),
      "ClientID should reset invalid cached IDs"
    );
    Assert.ok(
      Services.prefs.getPrefType(PREF_CACHED_PROFILEGROUPID) ==
        Ci.nsIPrefBranch.PREF_INVALID,
      "ClientID should reset invalid cached IDs"
    );
  }
});

add_task(async function test_set_profile_group_id() {
  // If there is no DRS file, and no cached id, we should get a new profile group ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let clientID = await ClientID.getClientID();
  let profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(typeof profileGroupID, "string");
  Assert.ok(uuidRegex.test(profileGroupID));

  await Assert.rejects(
    ClientID.setProfileGroupID("INVALID-UUID"),
    /Invalid profile group ID/,
    "Invalid profile group IDs aren't accepted"
  );

  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    profileGroupID,
    "Cached profile group ID should not have changed."
  );
  Assert.equal(
    await ClientID.getProfileGroupID(),
    profileGroupID,
    "Group ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  let validProfileGroupID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  await ClientID.setProfileGroupID(validProfileGroupID);

  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    validProfileGroupID,
    "Cached profile group ID should have changed."
  );
  Assert.equal(
    await ClientID.getProfileGroupID(),
    validProfileGroupID,
    "Group ID should have changed."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(
      validProfileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // New profile group ID should be stored in the cache.
  await ClientID._reset();
  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    validProfileGroupID,
    "Cached profile group ID be correct."
  );

  // New profile group ID should have been saved in the DRS file.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);

  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    null,
    "Cached profile group ID should not be available."
  );
  Assert.equal(
    await ClientID.getProfileGroupID(),
    validProfileGroupID,
    "Group ID should be correct."
  );
  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    validProfileGroupID,
    "Cached profile group ID be correct."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(
      validProfileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // And recoverable from the cache
  await ClientID._reset();
  await IOUtils.remove(drsPath, { ignoreAbsent: true });

  Assert.equal(
    ClientID.getCachedProfileGroupID(),
    validProfileGroupID,
    "Cached profile group ID be correct."
  );
  Assert.equal(
    await ClientID.getProfileGroupID(),
    validProfileGroupID,
    "Group ID should be correct."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(
      validProfileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }
});

add_task(async function test_setCanaryClientID() {
  const KNOWN_UUID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

  await ClientID._reset();

  // We should be able to set a valid UUID
  await ClientID.setCanaryClientID();
  let clientID = await ClientID.getClientID();
  Assert.equal(KNOWN_UUID, clientID);
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
  }
});

add_task(async function test_removeParallelGet() {
  // We should get a valid UUID after reset
  await ClientID.removeClientID();
  let firstClientID = await ClientID.getClientID();
  if (AppConstants.platform != "android") {
    Assert.equal(firstClientID, Glean.legacyTelemetry.clientId.testGetValue());
  }

  // We should get the same ID twice when requesting it in parallel to a reset.
  let promiseRemoveClientID = ClientID.removeClientID();
  let p = ClientID.getClientID();
  let newClientID = await ClientID.getClientID();
  await promiseRemoveClientID;
  let otherClientID = await p;

  Assert.notEqual(
    firstClientID,
    newClientID,
    "After reset client ID should be different."
  );
  Assert.equal(
    newClientID,
    otherClientID,
    "Getting the client ID in parallel to a reset should give the same id."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(newClientID, Glean.legacyTelemetry.clientId.testGetValue());
  }
});
