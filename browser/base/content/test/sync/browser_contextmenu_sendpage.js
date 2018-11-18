/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const targetsFixture = [ { id: 1, name: "Foo"}, { id: 2, name: "Bar"} ];

add_task(async function setup() {
  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
  sinon.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
});

add_task(async function test_page_contextmenu() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: targetsFixture,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Foo" },
    { label: "Bar" },
    "----",
    { label: "Send to All Devices" },
  ]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_link_contextmenu() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: targetsFixture,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });
  let expectation = sandbox.mock(gSync)
                           .expects("sendTabToDevice")
                           .once()
                           .withExactArgs("https://www.example.org/", [{id: 1, name: "Foo"}], "Click on me!!");

  // Add a link to the page
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    let a = content.document.createElement("a");
    a.href = "https://www.example.org";
    a.id = "testingLink";
    a.textContent = "Click on me!!";
    content.document.body.appendChild(a);
  });

  await openContentContextMenu("#testingLink", "context-sendlinktodevice", "context-sendlinktodevice-popup");
  is(document.getElementById("context-sendlinktodevice").hidden, false, "Send link to device is shown");
  is(document.getElementById("context-sendlinktodevice").disabled, false, "Send link to device is enabled");
  document.getElementById("context-sendlinktodevice-popup").querySelector("menuitem").click();
  await hideContentContextMenu();

  expectation.verify();
  sandbox.restore();
});

add_task(async function test_page_contextmenu_no_remote_clients() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: [],
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "No Devices Connected", disabled: true },
    "----",
    { label: "Connect Another Device..." },
    { label: "Learn About Sending Tabs..." },
  ]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_one_remote_client() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: [{ id: 1, name: "Foo"}],
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Foo" },
  ]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_sendable() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: targetsFixture,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: false });

  await openContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, true, "Send tab to device is disabled");
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_synced_yet() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: false, targets: [],
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  await openContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, true, "Send tab to device is disabled");
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_sync_not_ready_configured() {
  const sandbox = setupSendTabMocks({ syncReady: false, clientsSynced: false, targets: null,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  await openContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, true, "Send tab to device is disabled");
  checkPopup();
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_sync_not_ready_other_state() {
  const sandbox = setupSendTabMocks({ syncReady: false, clientsSynced: false, targets: null,
                                      state: UIState.STATUS_NOT_VERIFIED, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Account Not Verified", disabled: true },
    "----",
    { label: "Verify Your Account..." },
  ]);
  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_unconfigured() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: null,
                                      state: UIState.STATUS_NOT_CONFIGURED, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Not Connected to Sync", disabled: true },
    "----",
    { label: "Sign in to Sync..." },
    { label: "Learn About Sending Tabs..." },
  ]);

  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_not_verified() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, targets: null,
                                      state: UIState.STATUS_NOT_VERIFIED, isSendableURI: true });

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Account Not Verified", disabled: true },
    "----",
    { label: "Verify Your Account..." },
  ]);

  await hideContentContextMenu();

  sandbox.restore();
});

add_task(async function test_page_contextmenu_login_failed() {
  const syncReady = sinon.stub(gSync, "syncReady").get(() => true);
  const getState = sinon.stub(UIState, "get").returns({ status: UIState.STATUS_LOGIN_FAILED });
  const isSendableURI = sinon.stub(gSync, "isSendableURI").returns(true);

  await openContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  checkPopup([
    { label: "Account Not Verified", disabled: true },
    "----",
    { label: "Verify Your Account..." },
  ]);

  await hideContentContextMenu();

  syncReady.restore();
  getState.restore();
  isSendableURI.restore();
});

add_task(async function test_page_contextmenu_fxa_disabled() {
  const getter = sinon.stub(gSync, "SYNC_ENABLED").get(() => false);
  gSync.onSyncDisabled(); // Would have been called on gSync initialization if SYNC_ENABLED had been set.
  await openContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context-sep-sendpagetodevice").hidden, true, "Separator is also hidden");
  await hideContentContextMenu();
  getter.restore();
  [...document.querySelectorAll(".sync-ui-item")].forEach(e => e.hidden = false);
});

// We are not going to bother testing the visibility of context-sendlinktodevice
// since it uses the exact same code.
// However, browser_contextmenu.js contains tests that verify its presence.

add_task(async function teardown() {
  Weave.Service.clientsEngine.getClientType.restore();
  gBrowser.removeCurrentTab();
});

function checkPopup(expectedItems = null) {
  const popup = document.getElementById("context-sendpagetodevice-popup");
  if (!expectedItems) {
    is(popup.state, "closed", "Popup should be hidden.");
    return;
  }
  const menuItems = popup.children;
  is(menuItems.length, expectedItems.length, "Popup has the expected children count.");
  for (let i = 0; i < menuItems.length; i++) {
    const menuItem = menuItems[i];
    const expectedItem = expectedItems[i];
    if (expectedItem === "----") {
      is(menuItem.nodeName, "menuseparator", "Found a separator");
      continue;
    }
    is(menuItem.nodeName, "menuitem", "Found a menu item");
    // Bug workaround, menuItem.label "…" encoding is different than ours.
    is(menuItem.label.normalize("NFKC"), expectedItem.label, "Correct menu item label");
    is(menuItem.disabled, !!expectedItem.disabled, "Correct menu item disabled state");
  }
}

async function openContentContextMenu(selector, openSubmenuId = null) {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");

  const awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse(selector, 0, 0, {
      type: "contextmenu",
      button: 2,
      shiftkey: false,
      centered: true,
    },
    gBrowser.selectedBrowser);
  await awaitPopupShown;

  if (openSubmenuId) {
    const menuPopup = document.getElementById(openSubmenuId).menupopup;
    const menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
    menuPopup.openPopup();
    await menuPopupPromise;
  }
}

async function hideContentContextMenu() {
  const contextMenu = document.getElementById("contentAreaContextMenu");
  const awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await awaitPopupHidden;
}
