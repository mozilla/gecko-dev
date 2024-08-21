/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_updateDefaultProfileOnWindowSwitch() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = await setupMockDB();
  let rootDir = await profile.rootDir;

  const toolkitProfileObject = { storeID, rootDir };
  SelectableProfileService.groupToolkitProfile = toolkitProfileObject;

  // re-initialize because we updated the rootDir
  await SelectableProfileService.uninit();
  await SelectableProfileService.init();

  toolkitProfileObject.rootDir = "some/path";
  ok(
    SelectableProfileService.currentProfile instanceof SelectableProfile,
    "The current selectable profile exists"
  );
  is(
    SelectableProfileService.toolkitProfileRootDir,
    toolkitProfileObject.rootDir,
    `The SelectableProfileService rootDir is ${toolkitProfileObject.rootDir}`
  );

  let w = await BrowserTestUtils.openNewBrowserWindow();
  w.focus();
  // Focus the original window so we get an "activate" event and update the toolkitProfile rootDir
  window.focus();

  await BrowserTestUtils.waitForCondition(() => {
    return SelectableProfileService.toolkitProfileRootDir.path === rootDir.path;
  }, `Waited for SelectableProfileService.toolkitProfileRootDir.path to be updated to ${rootDir.path}, instead got ${SelectableProfileService.toolkitProfileRootDir.path}`);

  is(
    SelectableProfileService.toolkitProfileRootDir.path,
    rootDir.path,
    `The SelectableProfileService rootDir is ${rootDir.path}`
  );

  await BrowserTestUtils.closeWindow(w);

  await SelectableProfileService.uninit();
});
