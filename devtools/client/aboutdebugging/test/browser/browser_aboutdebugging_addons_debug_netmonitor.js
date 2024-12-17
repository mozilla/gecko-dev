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

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * Cover usages of the Network Monitor panel when debugging a Web Extension.
 */
add_task(async function testWebExtensionsToolboxNetmonitor() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        document.body.innerText = "Background Page Body Test Content";
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  const monitor = await toolbox.selectTool("netmonitor");
  const { document: monitorDocument, store } = monitor.panelWin;

  const expectedURL = "https://example.org/?test_netmonitor=1";

  await toolbox.commands.scriptCommand.execute(`fetch("${expectedURL}");`);

  // NOTE: we need to filter the requests to the ones that we expect until
  // the network monitor is not yet filtering out the requests that are not
  // coming from an extension window or a descendent of an extension window,
  // in both oop and non-oop extension mode (filed as Bug 1442621).
  function getExpectedStoreRequests() {
    return Array.from(store.getState().requests.requests.values()).filter(
      request => request.url === expectedURL
    );
  }

  let requests;

  await waitFor(() => {
    requests = getExpectedStoreRequests();

    return requests.length == 1;
  });

  is(requests.length, 1, "Got one request logged");
  is(requests[0].method, "GET", "Got a GET request");
  is(requests[0].url, expectedURL, "Got the expected request url");

  info("Resend webextension request");
  const firstRequest =
    monitorDocument.querySelectorAll(".request-list-item")[0];
  const waitForHeaders = waitUntil(() =>
    monitorDocument.querySelector(".headers-overview")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
  await waitForHeaders;
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);
  await selectContextMenuItem(monitor, "request-list-context-edit-resend");

  await waitUntil(
    () =>
      monitorDocument.querySelector(".http-custom-request-panel") &&
      monitorDocument.querySelector("#http-custom-request-send-button")
        .disabled === false
  );
  monitorDocument.querySelector("#http-custom-request-send-button").click();

  await waitFor(() => {
    requests = getExpectedStoreRequests();

    return requests.length == 2;
  }, "Wait for resent request to be received");

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});
