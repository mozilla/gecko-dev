/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  featureEngineIdToFluentId: "chrome://global/content/ml/Utils.sys.mjs",
});

const RED_ICON_DATA =
  "iVBORw0KGgoAAAANSUhEUgAAABIAAAASCAIAAADZrBkAAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QYGEgw1XkM0ygAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAAYSURBVCjPY/zPQA5gYhjVNqptVNsg1wYAItkBI/GNR3YAAAAASUVORK5CYII=";
const IMAGE_ARRAYBUFFER_RED = imageBufferFromDataURI(RED_ICON_DATA);
const FEATURE_ICON = "chrome://branding/content/icon64.png";

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

async function getTestExtension({ id, withIcon }) {
  const testExt = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name: `name-${id}`,
      icons: withIcon ? { 16: "test-icon.png" } : undefined,
      browser_specific_settings: {
        gecko: { id },
      },
    },
    files: withIcon ? { "test-icon.png": IMAGE_ARRAYBUFFER_RED } : undefined,
  });
  await testExt.startup();
  return testExt;
}

let mockProvider;
let promptService;
add_setup(async function () {
  mockProvider = new MockProvider(["mlmodel"]);
  promptService = mockPromptService();
});

/**
 * Test that model hub provider is conditionally shown in the sidebar.
 */
add_task(async function testModelHubProvider() {
  let win = await loadInitialView("extension");
  let modelHubCategory = win.document
    .getElementById("categories")
    .getButtonByName("mlmodel");

  await BrowserTestUtils.waitForCondition(async () => {
    return modelHubCategory.hidden;
  }, "Wait for the mlmodel category button to be hidden");

  ok(modelHubCategory.hidden, "Model hub category is hidden");

  await closeView(win);
  mockProvider.createAddons([
    {
      id: "mockmodel1@tests.mozilla.org",
      name: "Model Mock 1",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
      usedByFirefoxFeatures: [],
      usedByAddonIds: [],
    },
    {
      id: "mockmodel2@tests.mozilla.org",
      name: "Model Mock 2",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
      usedByFirefoxFeatures: [],
      usedByAddonIds: [],
    },
  ]);
  win = await loadInitialView("extension");
  let doc = win.document;
  modelHubCategory = doc
    .getElementById("categories")
    .getButtonByName("mlmodel");

  await BrowserTestUtils.waitForCondition(async () => {
    return !modelHubCategory.hidden;
  }, "Wait for the mlmodel category button to not be hidden");

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

/**
 * Test model hub card in the list view.
 */
add_task(async function testModelHubCard() {
  const expectedTelemetryCount = 1;
  Services.fog.testResetFOG();
  const extWithIcon = await getTestExtension({
    id: "addon-with-icon@test-extension",
    withIcon: true,
  });
  const extWithoutIcon = await getTestExtension({
    id: "addon-without-icon@test-extension",
    withIcon: false,
  });
  const id1 = "mockmodel1-without-size@tests.mozilla.org";
  const id2 = "mockmodel2-with-size@tests.mozilla.org";

  mockProvider.createAddons([
    {
      id: id1,
      name: "Model Mock 1",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
      totalSize: undefined,
      // Testing a model using one of the expected Firefox features.
      usedByFirefoxFeatures: ["about-inference"],
      // Testing extension using the models (one with its own icon and
      // one without any icon).
      usedByAddonIds: [extWithIcon.id, extWithoutIcon.id],
    },
    {
      id: id2,
      name: "Model Mock 2",
      permissions: AddonManager.PERM_CAN_UNINSTALL,
      type: "mlmodel",
      totalSize: 5 * 1024 * 1024,
      // Testing that a Firefox feature that is mistakenly missing a
      // corresponding fluent id is omitted.
      usedByFirefoxFeatures: [
        "non-existing-feature",
        "smart-tab-embedding-engine",
      ],
      // Testing that a non existing extension is omitted.
      usedByAddonIds: ["non-existing@test-extension", extWithIcon.id],
    },
  ]);

  let win = await loadInitialView("mlmodel");

  info("Test list view telemetry");
  let listViewEvent = Glean.modelManagement.listView.testGetValue() || [];
  Assert.equal(
    listViewEvent.length,
    expectedTelemetryCount,
    "Got the expected listView telemetry"
  );

  // Card No Size
  let card1 = getAddonCard(win, id1);
  ok(card1, `Found addon card for model ${id1}`);
  verifyAddonCard(card1, {
    expectedTotalSize: "0 bytes",
    expectedUsedBy: [
      {
        iconURL: FEATURE_ICON,
        fluentId: featureEngineIdToFluentId("about-inference"),
      },
      {
        iconURL: /\/test-icon\.png$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithIcon.id}` },
      },
      {
        iconURL: /\/extensionGeneric.svg$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithoutIcon.id}` },
      },
    ],
  });

  // Card With Size
  let card2 = getAddonCard(win, id2);
  ok(card2, `Found addon card for model ${id2}`);
  verifyAddonCard(card2, {
    expectedTotalSize: "5.0 MB",
    expectedUsedBy: [
      {
        iconURL: FEATURE_ICON,
        fluentId: featureEngineIdToFluentId("smart-tab-embedding-engine"),
      },
      {
        iconURL: /\/test-icon\.png$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithIcon.id}` },
      },
    ],
  });

  await closeView(win);
  await extWithIcon.unload();
  await extWithoutIcon.unload();

  function verifyAddonCard(card, { expectedTotalSize, expectedUsedBy }) {
    ok(!card.hasAttribute("expanded"), "The list card is not expanded");

    let mlmodelTotalSizeBubble = card.querySelector(
      ".mlmodel-total-size-bubble"
    );
    ok(mlmodelTotalSizeBubble, "Expect to see the mlmodel total size bubble");

    is(
      mlmodelTotalSizeBubble?.textContent.trim(),
      expectedTotalSize,
      "Got the expected total size text"
    );

    let mlmodelRemoveAddonButton = card.querySelector(
      ".mlmodel-remove-addon-button"
    );

    ok(
      !mlmodelRemoveAddonButton,
      "Expect to not see the mlmodel remove addon button"
    );

    ok(
      BrowserTestUtils.isVisible(card.optionsButton),
      "Expect the card options button to be visible in the list view"
    );

    const listAdditionEl = card.querySelector("mlmodel-card-list-additions");
    ok(listAdditionEl, "Found mlmodel-card-list-additions element");
    const usedByEls =
      listAdditionEl.shadowRoot.querySelectorAll(".mlmodel-used-by");
    for (const [idx, usedBy] of expectedUsedBy.entries()) {
      info(`Verifying usedBy entry: ${JSON.stringify(usedBy)}\n`);
      const img = usedByEls[idx].querySelector("img");
      ok(img, "Found img tag for the icon url");
      if (usedBy.iconURL instanceof RegExp) {
        ok(
          usedBy.iconURL.test(img.src),
          `Expected icon url ${img.src} to match ${usedBy.iconURL}`
        );
      } else {
        Assert.equal(img.src, usedBy.iconURL, "Got the expected icon url");
      }
      const label = usedByEls[idx].querySelector("label");
      const fluentAttrs = label.ownerDocument.l10n.getAttributes(label);
      Assert.equal(
        fluentAttrs.id,
        usedBy.fluentId,
        "Got the expected fluent id"
      );
      if (usedBy.fluentArgs) {
        Assert.deepEqual(
          fluentAttrs.args,
          usedBy.fluentArgs,
          "Got the expected fluent args"
        );
      }
    }
    Assert.equal(
      usedByEls.length,
      expectedUsedBy.length,
      "Got the expected number of model usedBy elements"
    );
  }
});

/**
 * Test model hub expanded details.
 */
add_task(async function testModelHubDetails() {
  const extWithIcon = await getTestExtension({
    id: "addon-with-icon@test-extension",
    withIcon: true,
  });
  const extWithoutIcon = await getTestExtension({
    id: "addon-without-icon@test-extension",
    withIcon: false,
  });
  const id1 = "mockmodel1-without-size@tests.mozilla.org";
  const id2 = "mockmodel2-with-size@tests.mozilla.org";

  const mockModel1 = {
    id: id1,
    model: "hostname/org/model-mock-1",
    permissions: AddonManager.PERM_CAN_UNINSTALL,
    type: "mlmodel",
    totalSize: undefined,
    lastUsed: new Date("2023-10-01T12:00:00Z"),
    updateDate: new Date("2023-10-01T12:00:00Z"),
    modelHomepageURL: "https://huggingface.co/org/model-mock-1",
    modelIconURL: "chrome://mozapps/skin/extensions/extensionGeneric.svg",
    // Testing a model using one of the expected Firefox features.
    usedByFirefoxFeatures: ["about-inference"],
    // Testing extension using the models (one with its own icon and
    // one without any icon).
    usedByAddonIds: [extWithIcon.id, extWithoutIcon.id],
    engineIds: [],
  };
  const mockModel2 = {
    id: id2,
    model: "hostname/org/model-mock-2",
    permissions: AddonManager.PERM_CAN_UNINSTALL,
    type: "mlmodel",
    totalSize: 5 * 1024 * 1024,
    lastUsed: new Date("2023-10-01T12:00:00Z"),
    updateDate: new Date("2023-10-01T12:00:00Z"),
    modelHomepageURL: "https://huggingface.co/org/model-mock-2",
    modelIconURL: "", // testing that empty icon sets to defult svg
    // Testing that a Firefox feature that is mistakenly missing a
    // corresponding fluent id is omitted.
    usedByFirefoxFeatures: [
      "non-existing-feature",
      "smart-tab-embedding-engine",
    ],
    // Testing that a non existing extension is omitted.
    usedByAddonIds: ["non-existing@test-extension", extWithIcon.id],
    engineIds: [],
  };

  mockProvider.createAddons([mockModel1, mockModel2]);

  await verifyAddonCardDetails({
    id: id1,
    expectedTotalSize: "0 bytes",
    expectedLastUsed: mockModel1.lastUsed,
    expectedModelHomepageURL: mockModel1.modelHomepageURL,
    expectedModelIconURL:
      "chrome://mozapps/skin/extensions/extensionGeneric.svg",
    expectedUsedBy: [
      {
        iconURL: FEATURE_ICON,
        fluentId: featureEngineIdToFluentId("about-inference"),
      },
      {
        iconURL: /\/test-icon\.png$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithIcon.id}` },
      },
      {
        iconURL: /\/extensionGeneric.svg$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithoutIcon.id}` },
      },
    ],
  });
  await verifyAddonCardDetails({
    id: id2,
    expectedTotalSize: "5.0 MB",
    expectedLastUsed: mockModel2.lastUsed,
    expectedModelHomepageURL: mockModel2.modelHomepageURL,
    expectedModelIconURL:
      "chrome://mozapps/skin/extensions/extensionGeneric.svg",
    expectedUsedBy: [
      {
        iconURL: FEATURE_ICON,
        fluentId: featureEngineIdToFluentId("smart-tab-embedding-engine"),
      },
      {
        iconURL: /\/test-icon\.png$/,
        fluentId: "mlmodel-extension-label",
        fluentArgs: { extensionName: `name-${extWithIcon.id}` },
      },
    ],
  });

  async function verifyAddonCardDetails({
    id,
    expectedTotalSize,
    expectedLastUsed,
    expectedModelHomepageURL,
    expectedModelIconURL,
    expectedUsedBy,
  }) {
    Services.fog.testResetFOG();
    let win = await loadInitialView("mlmodel");

    // Get the list view card DOM element for the given addon id.
    let card = getAddonCard(win, id);
    ok(card, `Found addon card for model ${id}`);
    ok(!card.hasAttribute("expanded"), "list view card is not expanded");

    // Load the detail view.
    let loaded = waitForViewLoad(win);
    card.querySelector('[action="expand"]').click();
    await loaded;

    info("Test detials view telemetry");
    let detailsViewEvent =
      Glean.modelManagement.detailsView.testGetValue() || [];
    Assert.equal(
      detailsViewEvent.length,
      1,
      "Got the expected detailsView telemetry"
    );

    info("Test list management button click telemetry");
    let listItemManageEvent =
      Glean.modelManagement.listItemManage.testGetValue() || [];
    Assert.equal(
      listItemManageEvent.length,
      1,
      "Got the expected listItemManage telemetry"
    );

    // Get the detail view card DOM element for the given addon id.
    card = getAddonCard(win, id);
    ok(card.hasAttribute("expanded"), "detail view card is expanded");

    let mlmodelTotalSizeBubble = card.querySelector(
      ".mlmodel-total-size-bubble"
    );
    ok(
      !mlmodelTotalSizeBubble,
      "Expect to not see the mlmodel total size bubble"
    );

    let mlmodelRemoveAddonButton = card.querySelector(
      ".mlmodel-remove-addon-button"
    );
    ok(
      mlmodelRemoveAddonButton,
      "Expect to see the mlmodel remove addon button"
    );

    // Set response to cancel & Fire off Delete click
    promptService._response = 1;
    EventUtils.sendMouseEvent({ type: "click" }, mlmodelRemoveAddonButton);
    await BrowserTestUtils.waitForEvent(card, "remove-cancelled");
    info("Test cancel remove prompt telemetry");
    let cancelEvent =
      Glean.modelManagement.removeConfirmation.testGetValue() || [];
    Assert.equal(
      "cancel",
      cancelEvent[0].extra.action,
      "Got the expected removeConfirmation telemetry"
    );

    // Set response to confirm & Fire off Delete click
    promptService._response = 0;
    EventUtils.sendMouseEvent({ type: "click" }, mlmodelRemoveAddonButton);
    await BrowserTestUtils.waitForEvent(card, "remove");
    info("Test confirm remove prompt telemetry");
    let confirmEvent =
      Glean.modelManagement.removeConfirmation.testGetValue() || [];
    Assert.equal(
      "remove",
      confirmEvent[1].extra.action,
      "Got the expected removeConfirmation telemetry"
    );

    info("Test how many removes iniated telemetry");
    let removeInitiatedEvent =
      Glean.modelManagement.removeInitiated.testGetValue() || [];
    Assert.equal(
      removeInitiatedEvent.length,
      2,
      "Got the expected removeInitiated telemetry"
    );

    ok(
      !card.querySelector(".addon-detail-mlmodel").hidden,
      "Expect to see the mlmodel details"
    );

    ok(
      BrowserTestUtils.isHidden(card.optionsButton),
      "Expect the card options button to be hidden in the detail view"
    );

    // Check the model total size
    let totalsizeEl = card.querySelector(".addon-detail-row-mlmodel-totalsize");
    ok(totalsizeEl, "Expect to see the total size");
    is(
      totalsizeEl?.querySelector("span")?.textContent.trim(),
      expectedTotalSize,
      "Got the expected total size text"
    );

    // Check the last used date
    let lastUsedEl = card.querySelector(".addon-detail-row-mlmodel-lastused");
    ok(lastUsedEl, "Expect to see the last used date");
    is(
      lastUsedEl?.querySelector("span")?.textContent.trim(),
      expectedLastUsed.toLocaleDateString(undefined, {
        year: "numeric",
        month: "long",
        day: "numeric",
      }),
      "Got the expected last used date text"
    );

    // Check the model card link
    let modelCardEl = card.querySelector(".addon-detail-row-mlmodel-modelcard");
    ok(modelCardEl, "Expect to see the model card link");
    let modelHomepageURL = modelCardEl.querySelector("a");

    EventUtils.sendMouseEvent({ type: "click" }, modelHomepageURL);
    info("Test model card link telemetry");
    let extensionModelLinkEvent =
      Glean.modelManagement.extensionModelLink.testGetValue() || [];
    Assert.equal(
      extensionModelLinkEvent.length,
      1,
      "Got the expected extensionModelLink telemetry"
    );

    ok(modelHomepageURL, "Expect to see the model card link element");
    is(
      modelHomepageURL?.href,
      expectedModelHomepageURL,
      "Got the expected model card link"
    );

    // Check the model version
    let versionEl = card.querySelector(".addon-detail-row-version");
    ok(versionEl, "Expect to see the model version");

    // Check the model image
    let iconSrc = card.querySelector(".card-heading-icon").getAttribute("src");
    ok(iconSrc, "Expected to see model card image src");
    is(iconSrc, expectedModelIconURL, "Got expected model card icon value");

    const detailsEl = card.querySelector("addon-mlmodel-details");
    ok(detailsEl, "Found mlmodel-card-list-additions element");
    const usedByEls = detailsEl.querySelectorAll(".mlmodel-used-by");
    info(`found usedByEls: ${usedByEls.length}`);
    for (const [idx, usedBy] of expectedUsedBy.entries()) {
      info(`Verifying usedBy entry: ${JSON.stringify(usedBy)}\n`);
      const img = usedByEls[idx].querySelector("img");
      ok(img, "Found img tag for the icon url");
      if (usedBy.iconURL instanceof RegExp) {
        ok(
          usedBy.iconURL.test(img.src),
          `Expected icon url ${img.src} to match ${usedBy.iconURL}`
        );
      } else {
        Assert.equal(img.src, usedBy.iconURL, "Got the expected icon url");
      }
      const label = usedByEls[idx].querySelector("label");
      const fluentAttrs = label.ownerDocument.l10n.getAttributes(label);
      Assert.equal(
        fluentAttrs.id,
        usedBy.fluentId,
        "Got the expected fluent id"
      );
      if (usedBy.fluentArgs) {
        Assert.deepEqual(
          fluentAttrs.args,
          usedBy.fluentArgs,
          "Got the expected fluent args"
        );
      }
    }
    Assert.equal(
      usedByEls.length,
      expectedUsedBy.length,
      "Got the expected number of model usedBy elements"
    );

    await closeView(win);
  }

  await extWithIcon.unload();
  await extWithoutIcon.unload();
});
