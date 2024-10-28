/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from /toolkit/profile/test/xpcshell/head.js */

"use strict";

const { SelectableProfile } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfile.sys.mjs"
);

let gProfileServiceInitialised = false;

/**
 * Starts up the toolkit profile services and initialises it with a new default profile.
 */
function startProfileService() {
  if (gProfileServiceInitialised) {
    return;
  }

  gProfileServiceInitialised = true;
  selectStartupProfile();
}

function getSelectableProfileService() {
  const { SelectableProfileService } = ChromeUtils.importESModule(
    "resource:///modules/profiles/SelectableProfileService.sys.mjs"
  );
  return SelectableProfileService;
}

/**
 * Starts the selectable profile service and creates the group store for the
 * current profile.
 */
async function initSelectableProfileService() {
  startProfileService();

  const SelectableProfileService = getSelectableProfileService();

  await SelectableProfileService.init();
  await SelectableProfileService.maybeSetupDataStore();
}

function getRelativeProfilePath(path) {
  let relativePath = path.getRelativePath(
    Services.dirsvc.get("UAppData", Ci.nsIFile)
  );

  if (AppConstants.platform === "win") {
    relativePath = relativePath.replace("/", "\\");
  }

  return relativePath;
}

async function createTestProfile(profileData = {}) {
  const SelectableProfileService = getSelectableProfileService();

  let name = profileData.name ?? "Test";
  let path = profileData.path;

  if (!path) {
    path = await SelectableProfileService.createProfileDirs(name);
    await SelectableProfileService.createProfileInitialFiles(path);
    path = SelectableProfileService.getRelativeProfilePath(path);
  }

  return SelectableProfileService.insertProfile({
    avatar: profileData.avatar ?? "book",
    name,
    path,
    themeBg: profileData.themeBg ?? "var(--background-color-box)",
    themeFg: profileData.themeFg ?? "var(--text-color)",
    themeL10nId: profileData.themeL10nId ?? "default",
  });
}
