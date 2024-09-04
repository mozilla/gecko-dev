/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  extensionStorageSync: "resource://gre/modules/ExtensionStorageSync.sys.mjs",
  Service: "resource://services-sync/service.sys.mjs",
});

const { ExtensionStorageEngineBridge } = ChromeUtils.importESModule(
  "resource://services-sync/engines/extension-storage.sys.mjs"
);
const SYNC_QUOTA_BYTES = 102400;

add_task(async function setup_storage_sync() {
  // So that we can write to the profile directory.
  do_get_profile();
});

add_task(async function test_storage_sync_service() {
  const service = extensionStorageSync;
  {
    // mocking notifyListeners so we have access to the return value of `service.set`
    service.notifyListeners = (extId, changeSet) => {
      equal(extId, "ext-1");
      let expected = {
        hi: {
          newValue: "hello! 💖",
        },
        bye: {
          newValue: "adiós",
        },
      };

      deepEqual(
        [changeSet],
        [expected],
        "`set` should notify listeners about changes"
      );
    };

    let newValue = {
      hi: "hello! 💖",
      bye: "adiós",
    };

    // finalling calling `service.set` which asserts the deepEqual in the above mocked `notifyListeners`
    await service.set({ id: "ext-1" }, newValue);
  }

  {
    service.notifyListeners = (_extId, _changeSet) => {
      console.log(`NOTIFY LISTENERS`);
    };

    let expected = {
      hi: "hello! 💖",
    };

    let value = await service.get({ id: "ext-1" }, ["hi"]);
    deepEqual(value, expected, "`get` with key should return value");

    let expected2 = {
      hi: "hello! 💖",
      bye: "adiós",
    };

    let allValues = await service.get({ id: "ext-1" }, null);
    deepEqual(
      allValues,
      expected2,
      "`get` without a key should return all values"
    );
  }

  {
    service.notifyListeners = (extId, changeSet) => {
      console.log("notifyListeners", extId, changeSet);
    };

    let newValue = {
      hi: "hola! 👋",
    };

    await service.set({ id: "ext-2" }, newValue);
    await service.clear({ id: "ext-1" });
    let allValues = await service.get({ id: "ext-1" }, null);
    deepEqual(allValues, {}, "clear removed ext-1");

    let allValues2 = await service.get({ id: "ext-2" }, null);
    let expected = { hi: "hola! 👋" };
    deepEqual(allValues2, expected, "clear didn't remove ext-2");
    // We need to clear data for ext-2 too, so later tests don't fail due to
    // this data.
    await service.clear({ id: "ext-2" });
  }
});

add_task(async function test_storage_sync_bridged_engine() {
  let engine = new ExtensionStorageEngineBridge(Service);
  await engine.initialize();
  let area = engine._rustStore;

  info("Add some local items");
  await area.set("ext-1", JSON.stringify({ a: "abc" }));
  await area.set("ext-2", JSON.stringify({ b: "xyz" }));

  info("Start a sync");
  await engine._bridge.syncStarted();

  info("Store some incoming synced items");
  let incomingEnvelopesAsJSON = [
    {
      id: "guidAAA",
      modified: 0.1,
      payload: JSON.stringify({
        extId: "ext-2",
        data: JSON.stringify({
          c: 1234,
        }),
      }),
    },
    {
      id: "guidBBB",
      modified: 0.1,
      payload: JSON.stringify({
        extId: "ext-3",
        data: JSON.stringify({
          d: "new! ✨",
        }),
      }),
    },
  ].map(e => JSON.stringify(e));

  await engine._bridge.storeIncoming(incomingEnvelopesAsJSON);

  info("Merge");
  // Three levels of JSON wrapping: each outgoing envelope, the cleartext in
  // each envelope, and the extension storage data in each cleartext payload.
  let outgoingEnvelopesAsJSON = await engine._bridge.apply();
  let outgoingEnvelopes = outgoingEnvelopesAsJSON.map(json => JSON.parse(json));
  let parsedCleartexts = outgoingEnvelopes.map(e => JSON.parse(e.payload));
  let parsedData = parsedCleartexts.map(c => JSON.parse(c.data));

  let changes = (await area.getSyncedChanges()).map(change => {
    return {
      extId: change.extId,
      changes: JSON.parse(change.changes),
    };
  });

  deepEqual(
    changes,
    [
      {
        extId: "ext-2",
        changes: {
          c: { newValue: 1234 },
        },
      },
      {
        extId: "ext-3",
        changes: {
          d: { newValue: "new! ✨" },
        },
      },
    ],
    "Should return pending synced changes for observers"
  );

  // ext-1 doesn't exist remotely yet, so the Rust sync layer will generate
  // a GUID for it. We don't know what it is, so we find it by the extension
  // ID.
  let ext1Index = parsedCleartexts.findIndex(c => c.extId == "ext-1");
  greater(ext1Index, -1, "Should find envelope for ext-1");
  let ext1Guid = outgoingEnvelopes[ext1Index].id;

  // ext-2 has a remote GUID that we set in the test above.
  let ext2Index = outgoingEnvelopes.findIndex(c => c.id == "guidAAA");
  greater(ext2Index, -1, "Should find envelope for ext-2");

  equal(outgoingEnvelopes.length, 2, "Should upload ext-1 and ext-2");
  deepEqual(
    parsedData[ext1Index],
    {
      a: "abc",
    },
    "Should upload new data for ext-1"
  );
  deepEqual(
    parsedData[ext2Index],
    {
      b: "xyz",
      c: 1234,
    },
    "Should merge local and remote data for ext-2"
  );

  info("Mark all extensions as uploaded");
  // await promisify(engine.setUploaded, 0, [ext1Guid, "guidAAA"]);
  await engine._bridge.setUploaded(0, [ext1Guid, "guidAAA"]);

  info("Finish sync");
  // await promisify(engine.syncFinished);
  await engine._bridge.syncFinished();

  // Try fetching values for the remote-only extension we just synced.
  let ext3Value = await area.get("ext-3", "null");
  deepEqual(
    JSON.parse(ext3Value),
    {
      d: "new! ✨",
    },
    "Should return new keys for ext-3"
  );

  info("Try applying a second time");
  let secondApply = await engine._bridge.apply();
  deepEqual(secondApply, {}, "Shouldn't merge anything on second apply");

  info("Wipe all items");
  await engine._bridge.wipe();

  for (let extId of ["ext-1", "ext-2", "ext-3"]) {
    // `get` always returns an object, even if there are no keys for the
    // extension ID.
    let value = await area.get(extId, "null");
    deepEqual(
      JSON.parse(value),
      {},
      `Wipe should remove all values for ${extId}`
    );
  }
});

add_task(async function test_storage_sync_quota() {
  let engine = new ExtensionStorageEngineBridge(Service);
  await engine.initialize();
  let service = engine._rustStore;

  await engine._bridge.wipe();
  await service.set("ext-1", JSON.stringify({ x: "hi" }));
  await service.set("ext-1", JSON.stringify({ longer: "value" }));

  let v1 = await service.getBytesInUse("ext-1", '"x"');
  Assert.equal(v1, 5); // key len without quotes, value len with quotes.
  let v2 = await service.getBytesInUse("ext-1", "null");
  // 5 from 'x', plus 'longer' (6 for key, 7 for value = 13) = 18.
  Assert.equal(v2, 18);

  // Now set something greater than our quota.
  let expectedMsg = "QuotaError: Error";
  let msg;
  try {
    await service.set(
      "ext-1",
      JSON.stringify({
        big: "x".repeat(SYNC_QUOTA_BYTES),
      })
    );
  } catch (ex) {
    msg = ex.toString();
  } finally {
    Assert.equal(expectedMsg, msg);
  }
});
