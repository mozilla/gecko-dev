/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
AddonTestUtils.initMochitest(this);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

// Check that addon browsers can be reloaded via the toolbox reload shortcuts
add_task(async function testWebExtensionToolboxReload() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Build and install the add-on manually (without installTemporaryExtensionFromXPI)
  // in order to be able to make changes to the add-on files before reloads
  const manifest = JSON.stringify({
    manifest_version: 2,
    version: "1.0",
    name: ADDON_NAME,
    background: {
      scripts: ["background.js"],
    },
    sidebar_action: {
      default_title: "Sidebar",
      default_icon: {
        64: "icon.png",
      },
      default_panel: "sidebar.html",
    },
  });
  const files = {
    "manifest.json": manifest,
    "background.js": `
      fetch("data:text/html,fooo");
      console.log("background script executed " + Math.random());
      throw new Error("background exception")
    `,
    "sidebar.html": `<!DOCTYPE html>
      <html>
        <head>
          <meta charset="utf-8">
        </head>
        <body>
          Sidebar
        </body>
      </html>
    `,
  };
  const tempAddonDir = AddonTestUtils.tempDir.clone();
  const addonDir = await AddonTestUtils.promiseWriteFilesToExtension(
    tempAddonDir.path,
    ADDON_ID,
    files,
    true
  );

  const addon = await AddonManager.installTemporaryAddon(addonDir);

  // Select the debugger right away to avoid any noise coming from the inspector.
  await pushPref("devtools.toolbox.selectedTool", "webconsole");
  const { devtoolsDocument, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  ok(
    devtoolsDocument.querySelector(".qa-reload-button"),
    "Reload button is visible"
  );
  ok(
    !devtoolsDocument.querySelector(".qa-back-button"),
    "Back button is hidden"
  );
  ok(
    !devtoolsDocument.querySelector(".qa-forward-button"),
    "Forward button is hidden"
  );
  ok(
    !devtoolsDocument.querySelector(".debug-target-url-form"),
    "URL form is hidden"
  );
  ok(
    devtoolsDocument.getElementById("toolbox-meatball-menu-noautohide"),
    "Disable popup autohide button is displayed"
  );
  ok(
    !devtoolsDocument.getElementById(
      "toolbox-meatball-menu-pseudo-locale-accented"
    ),
    "Accented locale is not displayed (only on browser toolbox)"
  );

  // Ensure opening the netmonitor to handle requests before reloading
  await toolbox.selectTool("netmonitor");

  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info("Wait for the initial background message to appear in the console");
  const initialMessage = await waitFor(() =>
    findMessageByType(hud, "background script executed", ".console-api")
  );
  ok(initialMessage, "Found the expected message from the background script");

  await waitFor(
    () => findMessageByType(hud, "background exception", ".error"),
    "Found the background page exception"
  );

  let loadedTargets = 0;
  await toolbox.commands.resourceCommand.watchResources(
    [toolbox.commands.resourceCommand.TYPES.DOCUMENT_EVENT],
    {
      ignoreExistingResources: true,
      onAvailable(resources) {
        for (const resource of resources) {
          if (resource.name == "dom-complete") {
            loadedTargets++;
          }
        }
      },
    }
  );

  info("Reload the addon using a toolbox reload shortcut");
  toolbox.win.focus();
  synthesizeKeyShortcut(L10N.getStr("toolbox.reload.key"), toolbox.win);

  info("Wait until a new background log message is logged");
  const secondMessage = await waitFor(() => {
    const newMessage = findMessageByType(
      hud,
      "background script executed",
      ".console-api"
    );
    if (newMessage && newMessage !== initialMessage) {
      return newMessage;
    }
    return false;
  });

  await waitFor(
    () => loadedTargets == 2,
    "Wait for background and popup targets to be reloaded"
  );
  let menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  await waitFor(
    () => menuList.querySelectorAll(".command").length == 3,
    "Wait for fallback, background and sidebar documents to visible in the iframe dropdown"
  );

  info("Reload via the debug target info bar button");
  loadedTargets = 0;
  clickReload(devtoolsDocument);

  info("Wait until yet another background log message is logged");
  await waitFor(() => {
    const newMessage = findMessageByType(
      hud,
      "background script executed",
      ".console-api"
    );
    return (
      newMessage &&
      newMessage !== initialMessage &&
      newMessage !== secondMessage
    );
  });

  await waitFor(
    () => loadedTargets == 2,
    "Wait for background and popup targets to be reloaded"
  );
  menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  await waitFor(
    () => menuList.querySelectorAll(".command").length == 3,
    "Wait for fallback, background and sidebar documents to visible in the iframe dropdown"
  );

  // Verify that we got the data: URI request made from the background page
  const netmonitor = await toolbox.selectTool("netmonitor");
  is(
    netmonitor.panelWin.store.getState().requests.requests.length,
    1,
    "There is a the request for the data: URI done from the background page"
  );

  await toolbox.selectTool("webconsole");

  info("Introduce a typo in the manifest before reloading");
  loadedTargets = 0;
  // Add an unexpected character at the end of the JSON
  files["manifest.json"] += `x`;
  await AddonTestUtils.promiseWriteFilesToExtension(
    tempAddonDir.path,
    ADDON_ID,
    files,
    true
  );
  clickReload(devtoolsDocument);

  const notification = await waitFor(
    () => toolbox.doc.querySelector(`.notification[data-key="reload-error"]`),
    "Wait for the error to be shown"
  );
  Assert.stringContains(
    notification.textContent,
    "unexpected non-whitespace character after JSON data",
    "The error message is correct"
  );
  is(loadedTargets, 0, "No target is loaded when the add-on fails loading");

  info("Restore to the valid manifest and reload again");
  files["manifest.json"] = manifest;
  // Empty the background page to prevent throwing the exception
  files["background.js"] = `console.log("background log");`;
  await AddonTestUtils.promiseWriteFilesToExtension(
    tempAddonDir.path,
    ADDON_ID,
    files,
    true
  );
  clickReload(devtoolsDocument);

  await waitFor(
    () => !toolbox.doc.querySelector(`.notification[data-key="reload-error"]`),
    "Wait for the error to be hidden"
  );
  await waitFor(
    () => loadedTargets == 2,
    "Wait for background and popup targets to be reloaded"
  );
  await waitFor(
    () => findMessageByType(hud, "background log", ".console-api"),
    "Found the background page console api call"
  );
  ok(
    !findMessageByType(hud, "background exception", ".error"),
    "The background page exception is no longer visible"
  );
  is(
    netmonitor.panelWin.store.getState().requests.requests.length,
    0,
    "The netmonitor is also cleared"
  );

  info("Enable log persistance in the console");
  devtoolsDocument
    .querySelector(".webconsole-console-settings-menu-item-persistentLogs")
    .click();
  await waitUntil(
    () => hud.ui.wrapper.getStore().getState().ui.persistLogs === true
  );

  clickReload(devtoolsDocument);

  info("Wait for the reloaded message in the console");
  await waitFor(() => findMessageByType(hud, "Reloaded", ".navigationMarker"));

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await addon.uninstall();
  await removeTab(tab);
});

function clickReload(devtoolsDocument) {
  devtoolsDocument.querySelector(".qa-reload-button").click();
}
