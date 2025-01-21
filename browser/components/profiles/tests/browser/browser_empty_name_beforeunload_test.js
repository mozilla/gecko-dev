/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await initGroupDatabase();
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  // Create some profiles to ensure everything works properly.
  await SelectableProfileService.init();
  let profileData = {
    name: "Other Profile",
    avatar: "book",
    themeId: "default",
    themeFg: "var(--text-color)",
    themeBg: "var(--background-color-box)",
    path: "somewhere",
  };
  await SelectableProfileService.insertProfile(profileData);

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

const resetGlean = async () => {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();
};

const launchDialog = async tab => {
  // Note: we manually trigger the dialog, rather than using PromptTestUtils,
  // because we want to assert on the contents of the dialog.
  tab.linkedBrowser.asyncPermitUnload();
  let dialogMgr = gBrowser
    .getTabDialogBox(tab.linkedBrowser)
    .getContentDialogManager();
  await BrowserTestUtils.waitForCondition(
    () => dialogMgr.dialogs.length,
    "Waiting for the beforeunload dialog to be displayed"
  );
  let dialog = dialogMgr.dialogs[0];
  await dialog._dialogReady;
  return dialog;
};

const assertDialogContents = dialog => {
  let doc = dialog.frameContentWindow.document;
  let titleText = doc.querySelector("#titleText").textContent;
  Assert.equal("Leave without naming this profile?", titleText);
  let acceptButton = doc
    .getElementById("commonDialog")
    .shadowRoot.querySelector('button[dlgtype="accept"]');
  Assert.equal("Leave", acceptButton.label);
  let cancelLabel = doc
    .getElementById("commonDialog")
    .shadowRoot.querySelector('button[dlgtype="cancel"]').label;
  Assert.equal("Cancel", cancelLabel);
};

// The shouldAccept boolean determines if the accept or cancel button is
// clicked.
const dismissDialog = async (tab, dialog, shouldAccept) => {
  let doc = dialog.frameContentWindow.document;
  let acceptButton = doc
    .getElementById("commonDialog")
    .shadowRoot.querySelector('button[dlgtype="accept"]');
  let cancelButton = doc
    .getElementById("commonDialog")
    .shadowRoot.querySelector('button[dlgtype="cancel"]');
  let dialogMgr = gBrowser
    .getTabDialogBox(tab.linkedBrowser)
    .getContentDialogManager();

  if (shouldAccept) {
    acceptButton.click();
  } else {
    cancelButton.click();
  }

  await BrowserTestUtils.waitForCondition(
    () => !dialogMgr.dialogs.length,
    "Waiting for the beforeunload dialog to be removed"
  );
};

const cleanup = async (tab, cardSelector) => {
  // Instead of manually inserting text into the input in the content document,
  // just disconnect the beforeunload listener and close the tab.
  await tab.linkedBrowser.ownerGlobal.SpecialPowers.spawn(
    tab.linkedBrowser,
    [cardSelector],
    async selector => {
      let card = content.document.querySelector(selector);
      content.removeEventListener("beforeunload", card);
    }
  );
  BrowserTestUtils.removeTab(tab);
  tab = null;
};

add_task(async function test_newprofile_page_beforeunload_empty_name_accept() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await resetGlean();
  is(
    null,
    Glean.profilesNew.alert.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  // Set an empty name so we can trigger the beforeunload handler without
  // needing to do anything in the page.
  profile.name = "";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog, true);
  await assertGlean("profiles", "new", "alert", "leave");

  await cleanup(tab, "new-profile-card");
  await resetGlean();
});

add_task(async function test_newprofile_page_beforeunload_empty_name_cancel() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await resetGlean();
  is(
    null,
    Glean.profilesNew.alert.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  profile.name = "";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog, false);
  await assertGlean("profiles", "new", "alert", "cancel");

  await cleanup(tab, "new-profile-card");
  await resetGlean();
});

add_task(async function test_editprofile_page_beforeunload_empty_name_accept() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await resetGlean();
  is(
    null,
    Glean.profilesExisting.alert.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  profile.name = "";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:editprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog, true);
  await assertGlean("profiles", "existing", "alert", "leave");

  await cleanup(tab, "edit-profile-card");
  await resetGlean();
});

add_task(async function test_editprofile_page_beforeunload_empty_name_cancel() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  await resetGlean();
  is(
    null,
    Glean.profilesExisting.alert.testGetValue(),
    "We have not recorded any Glean data yet"
  );

  profile.name = "";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:editprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog, false);
  await assertGlean("profiles", "existing", "alert", "cancel");

  await cleanup(tab, "edit-profile-card");
  await resetGlean();
});
