/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const profileDir = do_get_profile();

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs"
);

const TEST_STORE_FILE_PATH = PathUtils.join(
  profileDir.path,
  "test-containers.json"
);

add_setup(function () {
  Services.fog.initializeFOG();
});

add_task(async function test_container_events() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();

  let cis =
    ContextualIdentityService.createNewInstanceForTesting(TEST_STORE_FILE_PATH);
  ok(!!cis, "We have our instance of ContextualIdentityService");

  Assert.equal(undefined, Glean.containers.containerCreated.testGetValue());

  // 1) containers_enabled
  Services.prefs.setBoolPref("privacy.userContext.enabled", true);
  await cis.observe(null, "nsPref:changed");
  const enabledEvent = Glean.containers.containersEnabled.testGetValue();
  Assert.ok(Array.isArray(enabledEvent));
  Assert.greater(enabledEvent.length, 0);
  Assert.equal(enabledEvent[0].extra.enabled, "true", "The feature is enabled");

  // 2) container_profile_loaded
  await cis.load();
  const loadedCount = cis.getPublicUserContextIds().length;
  const profileEvent = Glean.containers.containerProfileLoaded.testGetValue();
  Assert.ok(Array.isArray(profileEvent), "container_profile_loaded must exist");
  Assert.equal(profileEvent.length, 1);
  Assert.equal(
    profileEvent[0].extra.containers,
    loadedCount,
    "payload `containers` must match the containers number"
  );

  // 3) container_created
  let identity = cis.create("Test", "fingerprint", "blue");
  const createdEvent = Glean.containers.containerCreated.testGetValue();
  ok(Array.isArray(createdEvent), "container_created must exist");
  Assert.equal(createdEvent.length, 1);
  equal(
    createdEvent[0].extra.container_id,
    String(identity.userContextId),
    "container_created must have the correct userContextID"
  );

  // 4) container_modified
  Assert.ok(cis.update(identity.userContextId, "Test2", "briefcase", "orange"));
  const modifiedEvent = Glean.containers.containerModified.testGetValue();
  Assert.ok(Array.isArray(modifiedEvent), "container_modified exists");
  Assert.equal(modifiedEvent.length, 1);
  Assert.equal(
    modifiedEvent[0].extra.container_id,
    String(identity.userContextId),
    "container_modified has the correct userContextID"
  );

  // 5) container_deleted
  Assert.ok(cis.remove(identity.userContextId));

  const deletedEvent = Glean.containers.containerDeleted.testGetValue();
  Assert.ok(Array.isArray(deletedEvent), "container_deleted exists");
  Assert.equal(deletedEvent.length, 1);
  Assert.equal(
    deletedEvent[0].extra.container_id,
    String(identity.userContextId),
    "container_deleted has the correct userContextID"
  );
});
