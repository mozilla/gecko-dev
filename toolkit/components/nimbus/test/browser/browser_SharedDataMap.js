/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);
const { SharedDataMap } = ChromeUtils.importESModule(
  "resource://nimbus/lib/SharedDataMap.sys.mjs"
);

const KEY = "browser_shareddatamap.js";
const PATH = FileTestUtils.getTempFile("shared-data-map").path;

add_setup(() => {
  SpecialPowers.addTaskImport(
    "SharedDataMap",
    "resource://nimbus/lib/SharedDataMap.sys.mjs"
  );
  SpecialPowers.addTaskImport(
    "TestUtils",
    "resource://testing-common/TestUtils.sys.mjs"
  );
  SpecialPowers.addTaskImport(
    "sinon",
    "resource://testing-common/Sinon.sys.mjs"
  );

  Services.ppmm.releaseCachedProcesses();
});

function setupTest({ multiprocess = false } = {}) {
  const sdm = new SharedDataMap(KEY, { path: PATH });

  return {
    sdm,
    async cleanup() {
      sdm._removeEntriesByKeys(Object.keys(sdm._store.data));

      // Wait for the store to finish writing to disk, then delete the file on disk.
      await sdm._store.finalize();
      await IOUtils.remove(PATH);

      if (multiprocess) {
        // Ensure we shut down any cached processes for the next test.
        Services.ppmm.releaseCachedProcesses();
      }
    },
  };
}

add_task(async function testSetSaves() {
  const { sdm, cleanup } = setupTest();
  await sdm.init();

  sinon.spy(sdm._store, "saveSoon");

  sdm.set("foo", "bar");

  Assert.ok(
    sdm._store.saveSoon.calledOnce,
    "Should call saveSoon when setting a value"
  );

  await cleanup();
});

add_task(async function testUpdate() {
  const { sdm, cleanup } = setupTest();
  await sdm.init();

  sdm.set("foo", "foo");
  sdm.set("bar", "bar");

  Assert.equal(sdm.get("foo"), "foo");
  Assert.equal(sdm.get("bar"), "bar");

  sdm.set("foo", "baz");
  sdm.set("bar", "qux");

  Assert.equal(sdm.get("foo"), "baz");
  Assert.equal(sdm.get("bar"), "qux");

  sdm._removeEntriesByKeys(["bar", "qux"]);

  Assert.equal(sdm.get("foo"), "baz");
  Assert.equal(typeof sdm.get("bar"), "undefined");

  await cleanup();
});

add_task(async function testInitSafe() {
  const { sdm, cleanup } = setupTest();
  await sdm.init();

  sinon.stub(sdm._store, "load");
  sinon.replaceGetter(sdm._store, "data", () => {
    throw new Error("uh oh");
  });

  await sdm.init();
  Assert.ok(sdm._store.load.calledOnce, "should have called load");

  await cleanup();
}).skip();

add_task(async function testInitMultiple() {
  const { sdm, cleanup } = setupTest();

  sinon.spy(sdm._store, "load");

  await sdm.init();
  await sdm.ready();

  Assert.ok(sdm._store.load.calledOnce, "load called");

  await sdm.init();

  Assert.ok(sdm._store.load.calledOnce, "load called only once");

  await cleanup();
});

add_task(async function testChildInit() {
  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
    forceNewProcess: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [KEY], async key => {
    const childSdm = new SharedDataMap(key);

    Assert.ok(
      !Object.hasOwn(childSdm, "_store"),
      "child SharedDataMap does not have a store"
    );
  });

  Services.ppmm.releaseCachedProcesses();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testSetNotifiesParent() {
  const { sdm, cleanup } = setupTest();
  await sdm.init();

  const onUpdate = sinon.stub();
  sdm.on("parent-store-update:foo", onUpdate);
  sdm.set("foo", "bar");

  Assert.ok(
    onUpdate.calledOnceWithExactly("parent-store-update:foo", "bar"),
    "update event sent"
  );

  await cleanup();
});

add_task(async function testSetNotifiesChild() {
  const { sdm, cleanup } = setupTest({ multiprocess: true });
  await sdm.init();

  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Open a tab so we have a content process.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
    forceNewProcess: true,
  });

  const messagePrefix = "testSetNotifiesChild";

  const complete = SpecialPowers.spawn(
    tab.linkedBrowser,
    [KEY, messagePrefix],
    async (key, prefix) => {
      function waitForSharedDataChanged() {
        return new Promise(resolve => {
          const listener = () => {
            resolve();
            Services.cpmm.sharedData.removeEventListener("change", listener);
          };

          Services.cpmm.sharedData.addEventListener("change", listener);
        });
      }

      const childSdm = new SharedDataMap(key);
      const onUpdate = sinon.stub();
      childSdm.on("child-store-update:foo", onUpdate);
      await childSdm.ready();

      Assert.equal(
        typeof childSdm.get("foo"),
        "undefined",
        "child does not see value"
      );

      // Send the begin message and wait for the SharedData update.
      let sharedDataChanged = waitForSharedDataChanged();
      Services.cpmm.sendAsyncMessage(`${prefix}:begin`);
      await sharedDataChanged;

      // Wait for the update event to fire.
      await TestUtils.waitForCondition(
        () => onUpdate.callCount,
        "Child received an update event"
      );
      Assert.ok(
        onUpdate.calledOnceWith("child-store-update:foo", { bar: 1 }),
        "child received update event"
      );
      Assert.deepEqual(
        childSdm.get("foo"),
        { bar: 1 },
        "child sees updated value"
      );

      onUpdate.resetHistory();

      // Send the delete message and wait for the SharedData update.
      sharedDataChanged = waitForSharedDataChanged();
      Services.cpmm.sendAsyncMessage(`${prefix}:delete`);
      await sharedDataChanged;

      await TestUtils.waitForCondition(() => !childSdm.get("foo"));
      Assert.ok(onUpdate.notCalled, "onUpdate not called for deletions");
      Assert.equal(
        typeof childSdm.get("foo"),
        "undefined",
        "After deletion, foo is undefined"
      );
    }
  );

  // Wait for the child process to register its listeners.
  await waitForChildMessage(`${messagePrefix}:begin`);

  // Set the value in the parent and wait for the child to finish its assertions.
  const continuePromise = waitForChildMessage(`${messagePrefix}:delete`);

  // Flush immediately. Do not wait for idle dispatch.
  sdm.set("foo", { bar: 1 });
  Services.ppmm.sharedData.flush();
  await continuePromise;

  // Delete the entry. _deleteForTests flushes so we don't have to.
  sdm._deleteForTests("foo");

  // Wait for the child task to complete.
  await complete;

  BrowserTestUtils.removeTab(tab);

  await cleanup();
});

add_task(async function testGetFromChildExisting() {
  const { sdm, cleanup } = setupTest({ multiprocess: true });
  await sdm.init();

  sdm.set("foo", { bar: "baz", qux: ["quux"] });

  // Ensure that we re-serialize SharedData to make the content immediately
  // available to new processes.
  Services.ppmm.sharedData.flush();

  const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: browserWindow.gBrowser,
    url: "https://example.com",
    forceNewProcess: true,
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [KEY], async key => {
    const childSdm = new SharedDataMap(key);
    await childSdm.ready();

    Assert.deepEqual(
      childSdm.get("foo"),
      { bar: "baz", qux: ["quux"] },
      "child sees correct value"
    );
  });

  BrowserTestUtils.removeTab(tab);

  await cleanup();
});
