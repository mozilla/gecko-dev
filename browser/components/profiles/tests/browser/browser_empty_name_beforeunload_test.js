/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await initGroupDatabase();
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

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

const dismissDialog = async (tab, dialog) => {
  let doc = dialog.frameContentWindow.document;
  let acceptButton = doc
    .getElementById("commonDialog")
    .shadowRoot.querySelector('button[dlgtype="accept"]');
  let dialogMgr = gBrowser
    .getTabDialogBox(tab.linkedBrowser)
    .getContentDialogManager();
  acceptButton.click();
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

add_task(async function test_newprofile_page_beforeunload_empty_name() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  // Set an empty name so we can trigger the beforeunload handler without
  // needing to do anything in the page.
  profile.name = "";

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog);
  await cleanup(tab, "new-profile-card");
});

add_task(async function test_editprofile_page_beforeunload_empty_name() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");
  profile.name = "";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:editprofile"
  );
  let dialog = await launchDialog(tab);
  assertDialogContents(dialog);
  await dismissDialog(tab, dialog);
  await cleanup(tab, "edit-profile-card");
});
