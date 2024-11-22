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

const { AppTestDelegate } = ChromeUtils.importESModule(
  "resource://specialpowers/AppTestDelegate.sys.mjs"
);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * Check that the node picker can be used when dynamically navigating to a
 * webextension popup.
 */
add_task(async function testNodePickerInExtensionPopup() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Note that this extension should not define a background script in order to
  // reproduce the issue. Otherwise opening the popup does not trigger an auto
  // navigation from DevTools and you have to use the "Disable Popup Auto Hide"
  // feature which works around the bug tested here.
  const { extension } = await installTemporaryExtensionFromXPI(
    {
      extraProperties: {
        browser_action: {
          default_title: "WebExtension with popup",
          default_popup: "popup.html",
          default_area: "navbar",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <body>
            <div id="pick-me"
                 style="width:100px; height: 60px; background-color: #f5e8fc">
              Pick me!
            </div>
          </body>
        </html>
      `,
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const inspector = await toolbox.getPanel("inspector");

  info("Start the node picker");
  await toolbox.nodePicker.start();

  info("Open the webextension popup");
  const { promise: onNewTarget, resolve: resolveNewTarget } =
    Promise.withResolvers();
  const onAvailable = async ({ targetFront }) => {
    if (targetFront.url.endsWith("/popup.html")) {
      resolveNewTarget(targetFront);
    }
  };
  const { promise: onTargetSelected, resolve: resolveTargetSelected } =
    Promise.withResolvers();
  const { targetCommand } = toolbox.commands;
  const onSelected = async ({ targetFront }) => {
    resolveTargetSelected(targetFront);
  };
  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.FRAME],
    onAvailable,
    onSelected,
  });
  const onPanelOpened = AppTestDelegate.awaitExtensionPanel(window, extension);

  const onReloaded = inspector.once("reloaded");
  clickOnAddonWidget(ADDON_ID);
  await onPanelOpened;
  info("Wait for the target front related to the popup");
  await onNewTarget;
  // The popup document will automatically be selected and the inspector reloaded
  info("Wait for the inspector to be reloaded against the popup's document");
  await onReloaded;

  const popup = gBrowser.ownerDocument.querySelector(
    ".webextension-popup-browser"
  );

  info("Pick an element inside the webextension popup");
  // First mouse over the element to simulate real world events
  BrowserTestUtils.synthesizeMouseAtCenter(
    "#pick-me",
    { type: "mousemove" },
    popup.browsingContext
  );
  info("Wait fot the popup's target to be selected");
  await onTargetSelected;
  targetCommand.unwatchTargets({
    types: [targetCommand.TYPES.FRAME],
    onAvailable,
  });

  // Only then, click on the element to pick it
  const onNewNodeFront = inspector.selection.once("new-node-front");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "#pick-me",
    {},
    popup.browsingContext
  );
  // Picking the element in the popup will change the currently selected target and make the inspector load the popup document
  info("Wait for the popup's target to become the selected one");

  const nodeFront = await onNewNodeFront;
  is(nodeFront.id, "pick-me", "The expected node front was selected");

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});
