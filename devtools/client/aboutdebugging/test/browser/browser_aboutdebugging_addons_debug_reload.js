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

  await installTemporaryExtensionFromXPI(
    {
      background() {
        console.log("background script executed " + Math.random());
      },
      id: ADDON_ID,
      name: ADDON_NAME,
      extraProperties: {
        sidebar_action: {
          default_title: "Sidebar",
          default_icon: {
            64: "icon.png",
          },
          default_panel: "sidebar.html",
        },
      },
      files: {
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
      },
    },
    document
  );

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

  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info("Wait for the initial background message to appear in the console");
  const initialMessage = await waitFor(() =>
    findMessagesByType(hud, "background script executed", ".console-api")
  );
  ok(initialMessage, "Found the expected message from the background script");

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
    const newMessage = findMessagesByType(
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
    const newMessage = findMessagesByType(
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

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

function clickReload(devtoolsDocument) {
  devtoolsDocument.querySelector(".qa-reload-button").click();
}
