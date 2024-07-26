/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

const RUNTIME_ID = "test-runtime-id";
const DEVICE_NAME = "test device name";
const RUNTIME_NAME = "TestUsbApp";

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * Test opening and closing the remote addon toolbox twice.
 */
add_task(async function test_opening_profiler_dialog() {
  const { disconnect, mocks } = await connectToLocalFirefox({
    runtimeId: RUNTIME_ID,
    runtimeName: RUNTIME_NAME,
    deviceName: DEVICE_NAME,
  });
  const { document, tab, window } = await openAboutDebugging();

  // Note: here we can't use installTemporaryExtensionFromXPI because we are
  // using the local client as a fake USB client.
  // We stay on this-firefox to trigger the installation of the temporary
  // extension, and then move to the USB device where we should be able to
  // debug it.
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const xpiData = {
    background() {
      document.body.innerText = "Background Page Body Test Content";
    },
    id: ADDON_ID,
    name: ADDON_NAME,
  };
  const xpiFile = createTemporaryXPI(xpiData);
  await installTemporaryExtension(xpiFile, xpiData.name, document);

  mocks.emitUSBUpdate();
  await connectToRuntime(DEVICE_NAME, document);
  await selectRuntime(DEVICE_NAME, RUNTIME_NAME, document);

  info("Wait until the addon debug target appears");
  await waitUntil(() => findDebugTargetByText(xpiData.name, document));

  info("Open a toolbox to debug the addon");
  let { devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Re-open a toolbox to debug the same addon");
  ({ devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  ));

  info("Close the toolbox");
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  info("Click on the remove button for the temporary extension");
  const target = findDebugTargetByText(xpiData.name, document);
  const removeButton = target.querySelector(
    ".qa-temporary-extension-remove-button"
  );
  removeButton.click();

  info(
    "Wait until the debug target with the updated extension name disappears"
  );
  await waitUntil(() => !findDebugTargetByText(ADDON_NAME, document));

  info("Disconnect");
  await disconnect(document);
  await removeTab(tab);
});
