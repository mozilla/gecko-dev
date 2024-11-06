/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";
const PREF_CACHED_PROFILEGROUPID = "toolkit.telemetry.cachedProfileGroupID";
const PREF_CACHED_USAGE_PROFILEID = "datareporting.dau.cachedUsageProfileID";

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
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let clientID = await ClientID.getClientID();
  let profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(typeof profileGroupID, "string");
  Assert.ok(uuidRegex.test(profileGroupID));
  // A new client should have distinct client ID and profile group ID.
  Assert.notEqual(profileGroupID, clientID);
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
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

  const validProfileGroupID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  const validClientID = "d06361a2-67d8-4d41-b804-6fab6ddf5461";

  // An older version of DRS should reset the profile group ID to the client ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.writeJSON(drsPath, {
    clientID: validClientID,
    profileGroupID: validProfileGroupID,
  });
  clientID = await ClientID.getClientID();
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(typeof profileGroupID, "string");
  Assert.ok(uuidRegex.test(profileGroupID));
  Assert.equal(clientID, validClientID);
  Assert.equal(profileGroupID, clientID);
  if (AppConstants.platform != "android") {
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // Test that valid DRS actually works.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  await IOUtils.writeJSON(drsPath, {
    version: 2,
    clientID: validClientID,
    profileGroupID: validProfileGroupID,
  });
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(profileGroupID, validProfileGroupID);
  clientID = await ClientID.getClientID();
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
    version: 2,
    profileGroupID: validProfileGroupID,
  });
  clientID = await ClientID.getClientID();
  profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(clientID, validClientID);
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
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
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

add_task(async function test_setCanaryIdentifiers() {
  const KNOWN_CLIENT_UUID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";
  const KNOWN_PROFILE_GROUP_UUID = "decafdec-afde-cafd-ecaf-decafdecafde";

  await ClientID._reset();

  // `setCanaryIdentifiers` doesn't touch the Usage Profile ID.
  let usageProfileID = await ClientID.getUsageProfileID();

  // We should be able to set a valid UUID
  await ClientID.setCanaryIdentifiers();
  let clientID = await ClientID.getClientID();
  Assert.equal(KNOWN_CLIENT_UUID, clientID);
  let profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(KNOWN_PROFILE_GROUP_UUID, profileGroupID);
  if (AppConstants.platform != "android") {
    Assert.equal(clientID, Glean.legacyTelemetry.clientId.testGetValue());
    Assert.equal(
      profileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  let usageProfileID2 = await ClientID.getUsageProfileID();
  Assert.equal(usageProfileID, usageProfileID2);
});

add_task(async function test_setCanaryUsageProfileIdentifier() {
  const KNOWN_USAGE_PROFILEID = "beefbeef-beef-beef-beef-beeefbeefbee";

  await ClientID._reset();

  let clientID = await ClientID.getClientID();
  let profileGroupID = await ClientID.getProfileGroupID();

  await ClientID.setCanaryUsageProfileIdentifier();

  let usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(KNOWN_USAGE_PROFILEID, usageProfileID);

  let clientID2 = await ClientID.getClientID();
  let profileGroupID2 = await ClientID.getProfileGroupID();
  Assert.equal(clientID, clientID2);
  Assert.equal(profileGroupID, profileGroupID2);
});

add_task(async function test_removeParallelGet() {
  // We should get a valid UUID after reset
  await ClientID.resetIdentifiers();
  let firstClientID = await ClientID.getClientID();
  let firstProfileGroupID = await ClientID.getProfileGroupID();
  if (AppConstants.platform != "android") {
    Assert.equal(firstClientID, Glean.legacyTelemetry.clientId.testGetValue());
    Assert.equal(
      firstProfileGroupID,
      Glean.legacyTelemetry.profileGroupId.testGetValue()
    );
  }

  // The IDs should differ after being reset.
  Assert.notEqual(firstClientID, firstProfileGroupID);

  // We should get the same ID twice when requesting it in parallel to a reset.
  let promiseResetIdentifiers = ClientID.resetIdentifiers();
  let p = ClientID.getClientID();
  let newClientID = await ClientID.getClientID();
  await promiseResetIdentifiers;
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

add_task(async function test_usage_profile_id() {
  const invalidIDs = [
    [-1, "setIntPref"],
    [0.5, "setIntPref"],
    ["INVALID-UUID", "setStringPref"],
    [true, "setBoolPref"],
    ["", "setStringPref"],
    ["3d1e1560-682a-4043-8cf2-aaaaaaaaaaaZ", "setStringPref"],
  ];

  // If there is no DRS file, and no cached id, we should get a new Usage Profile ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let clientID = await ClientID.getClientID();
  let profileGroupId = await ClientID.getProfileGroupID();
  let usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(typeof usageProfileID, "string");
  Assert.ok(uuidRegex.test(usageProfileID));
  // A new client should have a Usage Profile ID distinct from client ID and profile group ID.
  Assert.notEqual(usageProfileID, clientID);
  Assert.notEqual(usageProfileID, profileGroupId);

  // We should be guarded against invalid DRS json.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
  await IOUtils.writeUTF8(drsPath, "abcd", {
    tmpPath: drsPath + ".tmp",
  });
  usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(typeof usageProfileID, "string");
  Assert.ok(uuidRegex.test(usageProfileID));

  // If the DRS data is broken, we should end up with the cached ID.
  let oldUsageProfileID = usageProfileID;
  for (let [invalidID] of invalidIDs) {
    await ClientID._reset();
    await IOUtils.writeJSON(drsPath, { clientID: invalidID });
    usageProfileID = await ClientID.getUsageProfileID();
    Assert.equal(usageProfileID, oldUsageProfileID);
  }

  const validProfileGroupID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  const validClientID = "d06361a2-67d8-4d41-b804-6fab6ddf5461";
  const validUsageProfileId = "4d38e1a4-5034-44b1-9683-c0d8f748ee24";

  // Test that valid DRS actually works.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
  await IOUtils.writeJSON(drsPath, {
    version: 2,
    clientID: validClientID,
    profileGroupID: validProfileGroupID,
    usageProfileID: validUsageProfileId,
  });
  usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(usageProfileID, validUsageProfileId);
  if (AppConstants.platform != "android") {
    Assert.equal(usageProfileID, Glean.usage.profileId.testGetValue());
  }

  // Test that valid DRS actually works when the client ID is missing.
  await ClientID._reset();
  await IOUtils.writeJSON(drsPath, {
    version: 2,
    profileGroupID: validProfileGroupID,
    usageProfileID: validUsageProfileId,
  });
  usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(usageProfileID, validUsageProfileId);
  if (AppConstants.platform != "android") {
    Assert.equal(usageProfileID, Glean.usage.profileId.testGetValue());
  }

  // Test that reloading a valid DRS works.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
  usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(usageProfileID, validUsageProfileId);
  if (AppConstants.platform != "android") {
    Assert.equal(usageProfileID, Glean.usage.profileId.testGetValue());
  }

  // Assure that cached IDs are being checked for validity.
  for (let [invalidID, prefFunc] of invalidIDs) {
    await ClientID._reset();
    Services.prefs[prefFunc](PREF_CACHED_USAGE_PROFILEID, invalidID);
    let cachedID = ClientID.getCachedUsageProfileID();
    Assert.strictEqual(
      cachedID,
      null,
      "ClientID should ignore invalid cached IDs"
    );
    Assert.ok(
      !Services.prefs.prefHasUserValue(PREF_CACHED_USAGE_PROFILEID),
      "ClientID should reset invalid cached IDs"
    );
    Assert.ok(
      Services.prefs.getPrefType(PREF_CACHED_USAGE_PROFILEID) ==
        Ci.nsIPrefBranch.PREF_INVALID,
      "ClientID should reset invalid cached IDs"
    );
  }
});

add_task(async function test_usage_profile_id_is_added() {
  // Test that valid DRS actually works and Usage Profile ID is generated once
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
  Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);

  const validClientID = "d06361a2-67d8-4d41-b804-6fab6ddf5461";
  const validProfileGroupID = "5afebd62-a33c-416c-b519-5c60fb988e8e";

  await IOUtils.writeJSON(drsPath, {
    version: 2,
    clientID: validClientID,
    profileGroupID: validProfileGroupID,
    // no Usage Profile ID set!
  });

  let clientID = await ClientID.getClientID();
  let profileGroupID = await ClientID.getProfileGroupID();
  Assert.equal(clientID, validClientID);
  Assert.equal(profileGroupID, validProfileGroupID);

  let usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(typeof usageProfileID, "string");
  Assert.ok(uuidRegex.test(usageProfileID));

  // A client should have distinct Usage Profile ID and client ID.
  Assert.notEqual(usageProfileID, clientID);
});

add_task(async function test_set_usage_profile_id() {
  // If there is no DRS file, and no cached id, we should get a new Usage Profile ID.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
  await IOUtils.remove(drsPath, { ignoreAbsent: true });
  let clientID = await ClientID.getClientID();
  let usageProfileID = await ClientID.getUsageProfileID();
  Assert.equal(typeof usageProfileID, "string");
  Assert.ok(uuidRegex.test(usageProfileID));

  await Assert.rejects(
    ClientID.setUsageProfileID("INVALID-UUID"),
    /Invalid Usage Profile ID/,
    "Invalid Usage Profile IDs aren't accepted"
  );

  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    usageProfileID,
    "Cached Usage Profile ID should not have changed."
  );
  Assert.equal(
    await ClientID.getUsageProfileID(),
    usageProfileID,
    "Usage Profile ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(usageProfileID, Glean.usage.profileId.testGetValue());
  }

  let validUsageProfileID = "5afebd62-a33c-416c-b519-5c60fb988e8e";
  await ClientID.setUsageProfileID(validUsageProfileID);

  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    validUsageProfileID,
    "Cached Usage Profile ID should have changed."
  );
  Assert.equal(
    await ClientID.getUsageProfileID(),
    validUsageProfileID,
    "Group ID should have changed."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(validUsageProfileID, Glean.usage.profileId.testGetValue());
  }

  // New Usage Profile ID should be stored in the cache.
  await ClientID._reset();
  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    validUsageProfileID,
    "Cached Usage Profile ID be correct."
  );

  // New Usage Profile ID should have been saved in the DRS file.
  await ClientID._reset();
  Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);

  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    null,
    "Cached Usage Profile ID should not be available."
  );
  Assert.equal(
    await ClientID.getUsageProfileID(),
    validUsageProfileID,
    "Usage Profile ID should be correct."
  );
  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    validUsageProfileID,
    "Cached Usage Profile ID be correct."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(validUsageProfileID, Glean.usage.profileId.testGetValue());
  }

  // And recoverable from the cache
  await ClientID._reset();
  await IOUtils.remove(drsPath, { ignoreAbsent: true });

  Assert.equal(
    ClientID.getCachedUsageProfileID(),
    validUsageProfileID,
    "Cached Usage Profile ID be correct."
  );
  Assert.equal(
    await ClientID.getUsageProfileID(),
    validUsageProfileID,
    "Usage Profile ID should be correct."
  );
  Assert.equal(
    await ClientID.getClientID(),
    clientID,
    "Client ID should not have changed."
  );
  if (AppConstants.platform != "android") {
    Assert.equal(validUsageProfileID, Glean.usage.profileId.testGetValue());
  }
});
