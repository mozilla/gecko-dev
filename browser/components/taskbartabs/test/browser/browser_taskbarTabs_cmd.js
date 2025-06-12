/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  Subprocess: "resource://gre/modules/Subprocess.sys.mjs",
  TaskbarTabs: "resource:///modules/taskbartabs/TaskbarTabs.sys.mjs",
  TaskbarTabsPin: "resource:///modules/taskbartabs/TaskbarTabsPin.sys.mjs",
  TaskbarTabsUtils: "resource:///modules/taskbartabs/TaskbarTabsUtils.sys.mjs",
});

sinon.stub(TaskbarTabsPin, "pinTaskbarTab");

let taskbarTab1;
let taskbarTab2;

add_setup(async () => {
  const url1 = Services.io.newURI("https://www.test.com");
  const userContextId1 = 0;
  taskbarTab1 = await TaskbarTabs.findOrCreateTaskbarTab(url1, userContextId1);
  sinon.resetHistory();
});

add_task(async function test_commandline_handling() {
  let exe = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;
  let profile = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

  // Test existing Taskbar Tab opens when launched via commandline.

  let args = [
    "-taskbar-tab",
    taskbarTab1.id,
    "-new-window",
    taskbarTab1.startUrl,
    "-profile",
    profile,
    "-container",
    taskbarTab1.userContextId,
  ];

  let winPromise = BrowserTestUtils.waitForNewWindow();
  await Subprocess.call({
    command: exe,
    arguments: args,
  });
  let winExisting = await winPromise;

  ok(
    TaskbarTabsUtils.isTaskbarTabWindow(winExisting),
    "The window opened via command line should be a Taskbar Tabs window."
  );
  is(
    TaskbarTabsUtils.getTaskbarTabIdFromWindow(winExisting),
    taskbarTab1.id,
    "The existing Taskbar Tab ID should be used."
  );
  ok(
    !TaskbarTabsPin.pinTaskbarTab.called,
    "Pinning should not be attempted when launching an existing Taskbar Tab."
  );

  // Test non-existant Taskbar Tab is reconstructed when launched via commandline.

  const startUrl = "https://www.subdomain.test.com";
  const userContextId = 1;
  const id = Services.uuid.generateUUID().toString().slice(1, -1);

  args = [
    "-taskbar-tab",
    id,
    "-new-window",
    startUrl,
    "-profile",
    profile,
    "-container",
    userContextId,
  ];

  winPromise = BrowserTestUtils.waitForNewWindow();
  await Subprocess.call({
    command: exe,
    arguments: args,
  });
  let winReconstruct = await winPromise;

  ok(
    TaskbarTabsUtils.isTaskbarTabWindow(winReconstruct),
    "The window opened via command line should be a Taskbar Tabs window."
  );

  let windowId = TaskbarTabsUtils.getTaskbarTabIdFromWindow(winReconstruct);
  isnot(
    windowId,
    taskbarTab1.id,
    "The existing Taskbar Tab ID should be used."
  );

  let registered = await TaskbarTabs.getTaskbarTab(windowId);
  ok(registered, "A new Taskbar Tab should have been registered.");

  ok(
    TaskbarTabsPin.pinTaskbarTab.called,
    "Pinning should not be attempted when launching an existing Taskbar Tab."
  );

  await Promise.all([
    BrowserTestUtils.closeWindow(winExisting),
    BrowserTestUtils.closeWindow(winReconstruct),
  ]);
});
