/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that icons for application provided engines are correctly
 * updated from remote settings.
 */

"use strict";

// A skeleton configuration that gets filled in from TESTS during `add_setup`.
let CONFIG = [
  { identifier: "engine_no_initial_icon" },
  { identifier: "engine_icon_updates" },
  { identifier: "engine_icon_not_local" },
  { identifier: "engine_icon_out_of_date" },
];

async function assertIconMatches(actualIconData, expectedIcon) {
  let expectedBuffer = new Uint8Array(await getFileDataBuffer(expectedIcon));

  Assert.equal(
    actualIconData.length,
    expectedBuffer.length,
    "Should have received matching buffer lengths for the expected icon"
  );
  Assert.ok(
    actualIconData.every((value, index) => value === expectedBuffer[index]),
    "Should have received matching data for the expected icon"
  );
}

async function assertEngineIcon(engineId, expectedIcon, size = 0) {
  // Default value for size is 0 to mimic XPCOM optional parameters.
  let engine = Services.search.getEngineById(engineId);
  let engineIconURL = await engine.getIconURL(size);

  if (expectedIcon) {
    Assert.notEqual(
      engineIconURL,
      null,
      "Should have an icon URL for the engine."
    );

    let response = await fetch(engineIconURL);
    let actualBuffer = new Uint8Array(await response.arrayBuffer());

    await assertIconMatches(actualBuffer, expectedIcon);
  } else {
    Assert.equal(
      engineIconURL,
      null,
      "Should not have an icon URL for the engine."
    );
  }
}

let originalIconId = Services.uuid.generateUUID().toString();
let client;

add_setup(async function setup() {
  useHttpServer();
  SearchTestUtils.useMockIdleService();

  client = RemoteSettings("search-config-icons");
  await client.db.clear();

  sinon
    .stub(RemoteSettingsUtils, "baseAttachmentsURL")
    .returns(`${gHttpURL}/icons/`);

  // Add some initial records and attachments into the remote settings collection.
  await insertRecordIntoCollection(client, {
    id: originalIconId,
    filename: "remoteIcon.ico",
    // This uses a wildcard match to test the icon is still applied correctly.
    engineIdentifiers: ["engine_icon_upd*"],
    imageSize: 16,
  });
  await insertRecordIntoCollection(client, {
    id: Services.uuid.generateUUID().toString(),
    filename: "svgIcon.svg",
    engineIdentifiers: ["engine_icon_updates"],
    imageSize: 32,
  });
  // This attachment is not cached, so we don't have it locally.
  await insertRecordIntoCollection(
    client,
    {
      id: Services.uuid.generateUUID().toString(),
      filename: "bigIcon.ico",
      engineIdentifiers: [
        // This also tests multiple engine idenifiers works.
        "enterprise",
        "next_generation",
        "engine_icon_not_local",
      ],
      imageSize: 16,
    },
    false
  );

  // Add a record that is out of date, and update it with a newer one, but don't
  // cache the attachment for the new one.
  let outOfDateRecordId = Services.uuid.generateUUID().toString();
  await insertRecordIntoCollection(
    client,
    {
      id: outOfDateRecordId,
      filename: "remoteIcon.ico",
      engineIdentifiers: ["engine_icon_out_of_date"],
      imageSize: 16,
      // 10 minutes ago.
      lastModified: Date.now() - 600000,
    },
    true
  );
  let { record } = await mockRecordWithAttachment({
    id: outOfDateRecordId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_icon_out_of_date"],
    imageSize: 16,
  });
  await client.db.update(record);
  await client.db.importChanges({}, record.lastModified);

  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();

  // Testing that an icon is not local generates a `Could not find {id}...`
  // message.
  consoleAllowList.push("Could not find");

  registerCleanupFunction(async () => {
    sinon.restore();
  });
});

add_task(async function test_icon_added_unknown_engine() {
  // If the engine is unknown, and this is a new icon, we should still download
  // the icon, in case the engine is added to the configuration later.
  let newIconId = Services.uuid.generateUUID().toString();

  let mock = await mockRecordWithAttachment({
    id: newIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_unknown"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [mock.record],
      updated: [],
      deleted: [],
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  let icon;
  await TestUtils.waitForCondition(async () => {
    try {
      icon = await client.attachments.get(mock.record);
    } catch (ex) {
      // Do nothing.
    }
    return !!icon;
  }, "Should have loaded the icon into the attachments store.");

  await assertIconMatches(new Uint8Array(icon.buffer), "bigIcon.ico");
});

add_task(async function test_icon_added_existing_engine() {
  // If the engine is unknown, and this is a new icon, we should still download
  // it, in case the engine is added to the configuration later.
  let newIconId = Services.uuid.generateUUID().toString();

  let mock = await mockRecordWithAttachment({
    id: newIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_no_initial_icon"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [mock.record],
      updated: [],
      deleted: [],
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  await promiseIconChanged;
  await assertEngineIcon("engine_no_initial_icon", "bigIcon.ico");
});

add_task(async function test_icon_updated() {
  // Test that when an update for an engine icon is received, the engine is
  // correctly updated.

  // Check the engine has the expected icon to start with.
  await assertEngineIcon("engine_icon_updates", "remoteIcon.ico");

  // Update the icon for the engine.
  let mock = await mockRecordWithAttachment({
    id: originalIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_icon_upd*"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [],
      updated: [{ new: mock.record }],
      deleted: [],
    },
  });
  SearchTestUtils.idleService._fireObservers("idle");

  await promiseIconChanged;
  await assertEngineIcon("engine_icon_updates", "bigIcon.ico");
  info("Other icon should be untouched.");
  await assertEngineIcon("engine_icon_updates", "svgIcon.svg", 32);
});

add_task(async function test_icon_not_local() {
  // Tests that a download is queued and triggered when the icon for an engine
  // is not in either the local dump nor the cache.

  await assertEngineIcon("engine_icon_not_local", null);

  // A download should have been queued, so fire idle to trigger it.
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  SearchTestUtils.idleService._fireObservers("idle");
  await promiseIconChanged;

  await assertEngineIcon("engine_icon_not_local", "bigIcon.ico");
});

add_task(async function test_icon_out_of_date() {
  // Tests that a download is queued and triggered when the icon for an engine
  // is not in either the local dump nor the cache.

  await assertEngineIcon("engine_icon_out_of_date", "remoteIcon.ico");

  // A download should have been queued, so fire idle to trigger it.
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  SearchTestUtils.idleService._fireObservers("idle");
  await promiseIconChanged;

  await assertEngineIcon("engine_icon_out_of_date", "bigIcon.ico");
});
