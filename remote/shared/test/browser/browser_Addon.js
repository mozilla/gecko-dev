/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Addon: "chrome://remote/content/shared/Addon.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
});

add_task(async function test_installWithPath() {
  const addonPath = getSupportFilePath("amosigned.xpi");
  const addonId = await lazy.Addon.installWithPath(addonPath, true, false);
  try {
    is(addonId, "amosigned-xpi@tests.mozilla.org");
  } finally {
    const addon = await lazy.AddonManager.getAddonByID(addonId);
    await addon.uninstall();
  }
});

add_task(async function test_installWithPath_failure() {
  await Assert.rejects(
    lazy.Addon.installWithPath("", true, false),
    /UnknownError: Expected absolute path/,
    "Expected error was returned"
  );
});

add_task(async function test_installWithBase64() {
  const addonPath = getSupportFilePath("amosigned.xpi");
  const addonBase64 = await readFileAsBase64(addonPath);
  const addonId = await lazy.Addon.installWithBase64(addonBase64, true, false);
  try {
    is(addonId, "amosigned-xpi@tests.mozilla.org");
  } finally {
    const addon = await lazy.AddonManager.getAddonByID(addonId);
    await addon.uninstall();
  }
});

add_task(async function test_installWithBase64_failure() {
  await Assert.rejects(
    lazy.Addon.installWithBase64("", true, false),
    /InvalidWebExtensionError: Could not install Add-on: Component returned failure code/,
    "Expected error was returned"
  );
});

add_task(async function test_uninstall() {
  const addonPath = getSupportFilePath("amosigned.xpi");
  const file = new lazy.FileUtils.File(addonPath);
  await lazy.AddonManager.installTemporaryAddon(file);
  is(await lazy.Addon.uninstall("amosigned-xpi@tests.mozilla.org"), undefined);
});

add_task(async function test_uninstall_failure() {
  await Assert.rejects(
    lazy.Addon.uninstall("test"),
    /NoSuchWebExtensionError: Add-on with ID "test" is not installed/,
    "Expected error was returned"
  );
});
