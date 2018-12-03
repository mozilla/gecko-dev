/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "test-devtools@mozilla.org";

const PACKAGED_ADDON_ID = "bug1273184@tests";
const PACKAGED_ADDON_NAME = "bug 1273184";

function getReloadButton(document, addonName) {
  const names = getInstalledAddonNames(document);
  const name = names.filter(element => element.textContent === addonName)[0];
  ok(name, `Found ${addonName} add-on in the list`);
  const targetElement = name.parentNode.parentNode;
  const reloadButton = targetElement.querySelector(".reload-button");
  info(`Found reload button for ${addonName}`);
  return reloadButton;
}

/**
 * Creates a web extension from scratch in a temporary location.
 * The object must be removed when you're finished working with it.
 */
class TempWebExt {
  constructor(addonId) {
    this.addonId = addonId;
    this.tmpDir = FileUtils.getDir("TmpD", ["browser_addons_reload"]);
    if (!this.tmpDir.exists()) {
      this.tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    this.sourceDir = this.tmpDir.clone();
    this.sourceDir.append(this.addonId);
    if (!this.sourceDir.exists()) {
      this.sourceDir.create(Ci.nsIFile.DIRECTORY_TYPE,
                           FileUtils.PERMS_DIRECTORY);
    }
  }

  writeManifest(manifestData) {
    const manifest = this.sourceDir.clone();
    manifest.append("manifest.json");
    if (manifest.exists()) {
      manifest.remove(true);
    }
    const fos = Cc["@mozilla.org/network/file-output-stream;1"]
                              .createInstance(Ci.nsIFileOutputStream);
    fos.init(manifest,
             FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
             FileUtils.MODE_TRUNCATE,
             FileUtils.PERMS_FILE, 0);

    const manifestString = JSON.stringify(manifestData);
    fos.write(manifestString, manifestString.length);
    fos.close();
  }

  remove() {
    return this.tmpDir.remove(true);
  }
}

// Remove in Bug 1497264
/*
add_task(async function reloadButtonReloadsAddon() {
  const ADDON_NAME = "test-devtools";
  const { tab, document, window } = await openAboutDebugging("addons");
  const { AboutDebugging } = window;
  await waitForInitialAddonList(document);
  await installAddon({
    document,
    path: "addons/unpacked/manifest.json",
    name: ADDON_NAME,
  });

  const reloadButton = getReloadButton(document, ADDON_NAME);
  is(reloadButton.title, "", "Reload button should not have a tooltip");
  const onInstalled = promiseAddonEvent("onInstalled");

  const reloaded = once(AboutDebugging, "addon-reload");

  // The list is updated twice:
  // - AddonManager's onInstalled event
  // - WebExtension's Management's startup event
  const onListUpdated = waitForNEvents(AboutDebugging, "addons-updated", 2);

  reloadButton.click();
  await reloaded;
  await onListUpdated;

  const [reloadedAddon] = await onInstalled;
  is(reloadedAddon.name, ADDON_NAME,
     "Add-on was reloaded: " + reloadedAddon.name);

  await tearDownAddon(AboutDebugging, reloadedAddon);
  await closeAboutDebugging(tab);
});
*/

add_task(async function reloadButtonRefreshesMetadata() {
  const { tab, document, window } = await openAboutDebugging("addons");
  const { AboutDebugging } = window;
  await waitForInitialAddonList(document);

  const manifestBase = {
    "manifest_version": 2,
    "name": "Temporary web extension",
    "version": "1.0",
    "applications": {
      "gecko": {
        "id": ADDON_ID,
      },
    },
  };

  const tempExt = new TempWebExt(ADDON_ID);
  tempExt.writeManifest(manifestBase);

  // List updated twice:
  // - AddonManager's onInstalled event
  // - WebExtension's Management's startup event.
  let onListUpdated = waitForNEvents(AboutDebugging, "addons-updated", 2);
  const onInstalled = promiseAddonEvent("onInstalled");
  await AddonManager.installTemporaryAddon(tempExt.sourceDir);
  await onListUpdated;

  info("Wait until addon onInstalled event is received");
  await onInstalled;

  info("Wait until addon appears in about:debugging#addons");
  await waitUntilAddonContainer("Temporary web extension", document);

  const newName = "Temporary web extension (updated)";
  tempExt.writeManifest(Object.assign({}, manifestBase, {name: newName}));

  // List updated twice:
  // - AddonManager's onInstalled event
  // - WebExtension's Management's startup event.
  onListUpdated = waitForNEvents(AboutDebugging, "addons-updated", 2);
  // Wait for the add-on list to be updated with the reloaded name.
  const onReInstall = promiseAddonEvent("onInstalled");
  const reloadButton = getReloadButton(document, manifestBase.name);
  const reloaded = once(AboutDebugging, "addon-reload");
  reloadButton.click();
  await reloaded;
  await onListUpdated;

  info("Wait until addon onInstalled event is received again");
  const [reloadedAddon] = await onReInstall;

  info("Wait until addon name is updated in about:debugging#addons");
  await waitUntilAddonContainer(newName, document);

  await tearDownAddon(AboutDebugging, reloadedAddon);
  tempExt.remove();
  await closeAboutDebugging(tab);
});

add_task(async function onlyTempInstalledAddonsCanBeReloaded() {
  const { tab, document, window } = await openAboutDebugging("addons");
  const { AboutDebugging } = window;
  await waitForInitialAddonList(document);

  // List updated twice:
  // - AddonManager's onInstalled event
  // - WebExtension's Management's startup event.
  const onListUpdated = waitForNEvents(AboutDebugging, "addons-updated", 2);
  await installAddonWithManager(getSupportsFile("addons/bug1273184.xpi").file);
  await onListUpdated;

  info("Wait until addon appears in about:debugging#addons");
  await waitUntilAddonContainer(PACKAGED_ADDON_NAME, document);

  info("Retrieved the installed addon from the addon manager");
  const addon = await getAddonByID(PACKAGED_ADDON_ID);
  is(addon.name, PACKAGED_ADDON_NAME, "Addon name is correct");

  const reloadButton = getReloadButton(document, addon.name);
  ok(!reloadButton, "There should not be a reload button");

  await tearDownAddon(AboutDebugging, addon);
  await closeAboutDebugging(tab);
});
