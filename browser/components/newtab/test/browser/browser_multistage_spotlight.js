/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Spotlight } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/Spotlight.sys.mjs"
);
const { PanelTestProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/PanelTestProvider.sys.mjs"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

async function waitForClick(selector, win) {
  await TestUtils.waitForCondition(() => win.document.querySelector(selector));
  win.document.querySelector(selector).click();
}

async function showDialog(dialogOptions) {
  Spotlight.showSpotlightDialog(
    dialogOptions.browser,
    dialogOptions.message,
    dialogOptions.dispatchStub
  );
  const [win] = await TestUtils.topicObserved("subdialog-loaded");
  return win;
}

async function dialogClosed(browser) {
  await TestUtils.waitForCondition(
    () => !browser.ownerGlobal.gDialogBox?.isOpen,
    "Waiting for dialog to close"
  );
}

add_task(async function test_specialAction() {
  const sandbox = sinon.createSandbox();
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  let dispatchStub = sandbox.stub();
  let browser = gBrowser.selectedBrowser;
  let specialActionStub = sandbox.stub(SpecialMessageActions, "handleAction");

  let win = await showDialog({ message, browser, dispatchStub });
  await waitForClick("button.primary", win);
  await win.close();

  Assert.equal(
    specialActionStub.callCount,
    1,
    "Should be called by primary action"
  );
  Assert.deepEqual(
    specialActionStub.firstCall.args[0],
    message.content.screens[0].content.primary_button.action,
    "Should be called with button action"
  );

  sandbox.restore();
});

add_task(async function test_embedded_import() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.internal-testing.enabled", true]],
  });
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "IMPORT_SETTINGS_EMBEDDED"
  );
  let browser = gBrowser.selectedBrowser;
  let win = await showDialog({ message, browser });
  let migrationWizardReady = BrowserTestUtils.waitForEvent(
    win,
    "MigrationWizard:Ready"
  );

  await TestUtils.waitForCondition(() =>
    win.document.querySelector("migration-wizard")
  );
  Assert.ok(
    win.document.querySelector("migration-wizard"),
    "Migration Wizard rendered"
  );

  await migrationWizardReady;

  let panelList = win.document
    .querySelector("migration-wizard")
    .openOrClosedShadowRoot.querySelector("panel-list");
  Assert.equal(panelList.tagName, "PANEL-LIST");
  Assert.equal(panelList.firstChild.tagName, "PANEL-ITEM");

  await win.close();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_embedded_browser() {
  const TEST_SCREEN = {
    id: "EMBEDDED_BROWSER",
    content: {
      tiles: {
        type: "embedded_browser",
        data: {
          style: {
            width: "100%",
            height: "200px",
          },
          url: "https://example.com/",
        },
      },
    },
  };
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  message.content.screens[0] = TEST_SCREEN;

  let browser = gBrowser.selectedBrowser;
  let win = await showDialog({ message, browser });

  await TestUtils.waitForCondition(() =>
    win.document.querySelector("div.embedded-browser-container")
  );

  const embeddedBrowser = win.document.querySelector(
    "div.embedded-browser-container browser"
  );
  Assert.ok(embeddedBrowser, "Embedded browser rendered");

  await TestUtils.waitForCondition(
    () => !embeddedBrowser.browsingContext.webProgress.isLoadingDocument
  );
  Assert.ok(embeddedBrowser.currentURI, "Should have a currentURI set.");

  Assert.equal(
    embeddedBrowser.currentURI.spec,
    TEST_SCREEN.content.tiles.data.url,
    "Embedded browser rendered with configured URL"
  );
  Assert.equal(
    embeddedBrowser.style.height,
    TEST_SCREEN.content.tiles.data.style.height,
    "Embedded browser rendered with configured height"
  );
  Assert.equal(
    embeddedBrowser.style.width,
    TEST_SCREEN.content.tiles.data.style.width,
    "Embedded browser rendered with configured width"
  );

  win.close();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_disableEscClose() {
  const sandbox = sinon.createSandbox();
  let message = (await PanelTestProvider.getMessages()).find(
    m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE"
  );
  message.content.disableEscClose = true;

  let browser = gBrowser.selectedBrowser;
  let stub = sandbox.stub();
  let win = await showDialog({ message, browser, stub });

  await TestUtils.waitForCondition(() =>
    win.document.querySelector("button.dismiss-button")
  );

  EventUtils.synthesizeKey(
    "KEY_Escape",
    { key: "Escape", code: "Escape" },
    win
  );

  Assert.ok(
    browser?.ownerGlobal.gDialogBox.isOpen,
    "Spotlight does not close with ESC key with 'disableEscClose' set to true"
  );

  win.document.querySelector("button.dismiss-button").click();

  await dialogClosed(browser);

  Assert.ok(
    true,
    "Spotlight closes on dismiss button click with 'disableEscClose' set to true"
  );

  await win.close();
  sandbox.restore();
});
