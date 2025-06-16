/* vim: set ts=2 sw=2 sts=2 et : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TaskbarTabsRegistry, TaskbarTabsRegistryStorage } =
  ChromeUtils.importESModule(
    "resource:///modules/taskbartabs/TaskbarTabsRegistry.sys.mjs"
  );

function testFile() {
  let path = do_get_tempdir();
  let filename = Services.uuid.generateUUID().toString().slice(1, -1);
  path.append(filename + ".json");

  registerCleanupFunction(() => {
    if (path.exists()) {
      path.remove(false);
    }
  });

  return path;
}

add_task(async function test_create_taskbar_tab() {
  const url = Services.io.newURI("https://www.test.com/start");
  const userContextId = 0; // Default container.

  const registry = new TaskbarTabsRegistry();

  Assert.ok(
    !registry.findTaskbarTab(url, userContextId),
    "Initially, no Taskbar Tab should exist for the given URL and container."
  );

  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);

  Assert.ok(taskbarTab, "Taskbar Tab should be created.");
  Assert.deepEqual(
    registry.findTaskbarTab(url, userContextId),
    taskbarTab,
    "Found Taskbar Tab should match the one returned on creation."
  );

  const secondUrl = Services.io.newURI("https://www.another-test.com/start");
  const secondUserContextId = 1;
  const secondTaskbarTab = registry.findOrCreateTaskbarTab(
    secondUrl,
    secondUserContextId
  );
  Assert.deepEqual(
    registry.findTaskbarTab(url, userContextId),
    taskbarTab,
    "First Taskbar Tab created should still be present."
  );
  Assert.deepEqual(
    registry.findTaskbarTab(secondUrl, secondUserContextId),
    secondTaskbarTab,
    "Second Taskbar Tab created should still be present."
  );

  const repeatTaskbarTab = registry.findOrCreateTaskbarTab(
    secondUrl,
    secondUserContextId
  );
  Assert.deepEqual(
    repeatTaskbarTab,
    secondTaskbarTab,
    "Should have found the second created Taskbar Tab instead of creating a new Taskbar Tab."
  );
});

add_task(async function test_remove_taskbar_tab() {
  const url = Services.io.newURI("https://www.test.com/start");
  const userContextId = 0;

  const registry = new TaskbarTabsRegistry();
  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);

  Assert.deepEqual(
    registry.findTaskbarTab(url, userContextId),
    taskbarTab,
    "Taskbar Tab ID should match the ID returned on creation."
  );

  registry.removeTaskbarTab(taskbarTab.id);

  Assert.ok(
    !registry.findTaskbarTab(url, userContextId),
    "Taskbar Tab ID should be removed."
  );
});

add_task(async function test_container_mismatch() {
  const url = Services.io.newURI("https://www.test.com/start");
  const userContextId = 0;
  const mismatchedUserContextId = 1;

  const registry = new TaskbarTabsRegistry();
  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);
  Assert.ok(taskbarTab, "Taskbar Tab ID should be created.");

  Assert.ok(
    !registry.findTaskbarTab(url, mismatchedUserContextId),
    `${mismatchedUserContextId} should not match a Taskbar Tab created with container ${userContextId}.`
  );
});

add_task(async function test_scope_navigable() {
  const url = Services.io.newURI("https://www.test.com/start");
  const validNavigationDomain = Services.io.newURI("https://www.test.com/test");
  const validNavigationSubdomain = Services.io.newURI(
    "https://www.subdomain.test.com"
  );
  const invalidNavigationDomain = Services.io.newURI(
    "https://www.anothertest.com"
  );
  const userContextId = 0;

  const registry = new TaskbarTabsRegistry();
  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);

  Assert.ok(
    taskbarTab.isScopeNavigable(validNavigationDomain),
    `${validNavigationDomain} should be a valid navigation target for ${taskbarTab.scopes[0].hostname}.`
  );

  Assert.ok(
    taskbarTab.isScopeNavigable(validNavigationSubdomain),
    `${validNavigationSubdomain} should be a valid navigation target for ${taskbarTab.scopes[0].hostname}.`
  );
  Assert.ok(
    !taskbarTab.isScopeNavigable(invalidNavigationDomain),
    `${invalidNavigationDomain} should be a valid navigation target for ${taskbarTab.scopes[0].hostname}.`
  );
});

add_task(async function test_psl_navigable() {
  const url = Services.io.newURI("https://www.bmoattachments.org/start");
  const invalidNavigationPublicSuffixList = Services.io.newURI(
    "https://www.invalid.bmoattachments.org"
  );
  const userContextId = 0;

  const registry = new TaskbarTabsRegistry();
  const taskbarTab = registry.findOrCreateTaskbarTab(url, userContextId);

  Assert.ok(
    !taskbarTab.isScopeNavigable(invalidNavigationPublicSuffixList),
    `bmoattachments.org is on the Public Suffix List, therefore ${invalidNavigationPublicSuffixList} should not be a valid navigation target for ${taskbarTab.scopes[0].hostname}.`
  );
});

add_task(async function test_save_and_load_consistency() {
  const url = Services.io.newURI("https://www.test.com/start");
  const userContextId = 0;

  let saveRegistry = new TaskbarTabsRegistry();
  const saveTaskbarTab = saveRegistry.findOrCreateTaskbarTab(
    url,
    userContextId
  );

  let file = testFile();
  let storage = new TaskbarTabsRegistryStorage(saveRegistry, file);
  await storage.save();

  const loadRegistry = await TaskbarTabsRegistry.create({ loadFile: file });
  let loadTaskbarTab = loadRegistry.getTaskbarTab(saveTaskbarTab.id);

  Assert.deepEqual(
    saveTaskbarTab,
    loadTaskbarTab,
    "Taskbar Tab object should be identical after save and load."
  );
});

add_task(async function test_load_and_save_consistency() {
  const loadFile = do_get_file("test_taskbarTabs.json");

  // Test loading from the mock file
  const registry = await TaskbarTabsRegistry.create({ loadFile });
  Assert.equal(
    registry.findTaskbarTab(Services.io.newURI("https://www.test.com"), 0).id,
    "4186657a-0fe5-492a-af64-dc628c232c4c",
    "Taskbar Tab ID should match the one in the test JSON file."
  );

  // Test saving to a new file
  let file = testFile();
  let storage = new TaskbarTabsRegistryStorage(registry, file);
  await storage.save();

  // Verify the output against the original file on disk.
  const originalData = await IOUtils.readJSON(loadFile.path);
  const outputData = await IOUtils.readJSON(file.path);

  Assert.deepEqual(
    outputData,
    originalData,
    "The in-memory mock file output should match the original file on disk."
  );
});
