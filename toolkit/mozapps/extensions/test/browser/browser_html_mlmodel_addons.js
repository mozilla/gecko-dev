/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let mockProvider;
add_setup(async function () {
  mockProvider = new MockProvider(["mlmodel"]);
});

/**
 * Test that model hub provider is conditionally shown in the sidebar.
 */
add_task(async function testModelHubProvider() {
  let win = await loadInitialView("extension");
  let modelHubCategory = win.document
    .getElementById("categories")
    .getButtonByName("mlmodel");

  ok(modelHubCategory.hidden, "Model hub category is hidden");

  await closeView(win);
  mockProvider.createAddons([
    {
      id: "mockmodel1@tests.mozilla.org",
      name: "Model Mock 1",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
    },
    {
      id: "mockmodel2@tests.mozilla.org",
      name: "Model Mock 2",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
    },
  ]);
  win = await loadInitialView("extension");
  let doc = win.document;
  modelHubCategory = doc
    .getElementById("categories")
    .getButtonByName("mlmodel");

  ok(!modelHubCategory.hidden, "Model hub category is shown");

  let mlmodelLoaded = waitForViewLoad(win);
  modelHubCategory.click();
  await mlmodelLoaded;

  let enabledSection = getSection(doc, "mlmodel-enabled-section");
  is(
    enabledSection.children.length,
    2,
    "Got the expected number of mlmodel entries"
  );
  ok(
    !enabledSection.querySelector(".list-section-heading"),
    "No heading for mlmodel"
  );
  is(
    enabledSection.previousSibling.localName,
    "mlmodel-list-intro",
    "Model hub custom section is shown"
  );

  let promiseListUpdated = BrowserTestUtils.waitForEvent(
    enabledSection.closest("addon-list"),
    "remove"
  );
  info("Uninstall one of the mlmodel entries");
  const mlmodelmock2 = await AddonManager.getAddonByID(
    "mockmodel2@tests.mozilla.org"
  );

  await mlmodelmock2.uninstall();
  info("Wait for the list of mlmodel entries to be updated");
  await promiseListUpdated;

  enabledSection = getSection(doc, "mlmodel-enabled-section");
  is(
    enabledSection.children.length,
    1,
    "Got the expected number of mlmodel entries"
  );

  promiseListUpdated = BrowserTestUtils.waitForEvent(
    enabledSection.closest("addon-list"),
    "remove"
  );
  info("Uninstall the last one of the mlmodel entries");
  const mlmodelmock1 = await AddonManager.getAddonByID(
    "mockmodel1@tests.mozilla.org"
  );

  await mlmodelmock1.uninstall();
  info("Wait for the list of mlmodel entries to be updated");
  await promiseListUpdated;

  enabledSection = getSection(doc, "mlmodel-enabled-section");
  is(
    enabledSection.children.length,
    0,
    "Expect mlmodel add-ons list view to be empty"
  );

  let emptyMessageFindModeOnAMO = doc.querySelector("#empty-addons-message");
  is(
    emptyMessageFindModeOnAMO,
    null,
    "Expect no #empty-addons-message element in the empty mlmodel list view"
  );

  await closeView(win);
});
