/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

/* exported ORIGINAL_SCRIPT */
const ORIGINAL_SCRIPT = `
const div = document.createElement("div");
div.id = "created-by-original";
document.body.appendChild(div);
`;

/* exported OVERRIDDEN_SCRIPT */
const OVERRIDDEN_SCRIPT = `
const div = document.createElement("div");
div.id = "created-by-override";
document.body.appendChild(div);
`;

/* exported ORIGINAL_STYLESHEET */
const ORIGINAL_STYLESHEET = `
  body {
    color: red;
  }
`;

/* exported OVERRIDDEN_STYLESHEET */
const OVERRIDDEN_STYLESHEET = `
  body {
    color: blue;
  }
`;

/* exported ORIGINAL_HTML */
const ORIGINAL_HTML = `<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8"/>
    <title>Original title</title>
  </head>
  <body>
    Original content
    <script type="text/javascript" src="/script.js"></script>
    <style>@import "/style.css"</style>
  </body>
</html>`;

/* exported OVERRIDDEN_HTML */
const OVERRIDDEN_HTML = `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>Overridden title</title>
</head>
<body>
  Overridden content
</body>
</html>
`;

function startOverridesHTTPServer() {
  const httpServer = createTestHTTPServer();
  const baseURL = `http://localhost:${httpServer.identity.primaryPort}/`;

  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");

  httpServer.registerPathHandler("/index.html", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(ORIGINAL_HTML);
  });

  httpServer.registerPathHandler("/script.js", (request, response) => {
    response.setHeader("Content-Type", "application/javascript");
    response.write(ORIGINAL_SCRIPT);
  });

  httpServer.registerPathHandler("/style.css", (request, response) => {
    response.setHeader("Content-Type", "text/css; charset=utf-8");
    response.write(ORIGINAL_STYLESHEET);
  });

  return baseURL;
}

/* exported assertOverrideColumnStatus */
/**
 * Check if the override column is currently visible or not.
 *
 * @param {object} monitor
 *     The netmonitor monitor instance.
 * @param {object} options
 * @param {boolean} options.visible
 *     Whether the column is expected to be visible or not.
 */
async function assertOverrideColumnStatus(monitor, { visible }) {
  const doc = monitor.panelWin.document;

  // Assert override column is hidden
  is(
    !!doc.querySelector(`#requests-list-override-button`),
    visible,
    `Column Override should be ${visible ? "visible" : "hidden"}`
  );

  // Assert override column context menu item status.
  const overrideColumnToggle = await openContextMenuForItem(
    monitor,
    // Trigger the column context menu on the status column which should usually
    // be available.
    doc.querySelector("#requests-list-status-button"),
    "request-list-header-override-toggle"
  );

  if (visible) {
    is(
      overrideColumnToggle.getAttribute("checked"),
      "true",
      "The Override column menu item is checked"
    );
  } else {
    ok(
      !overrideColumnToggle.getAttribute("checked"),
      "The Override column menu item is unchecked"
    );
  }

  // Assert that override column is always disabled
  is(
    overrideColumnToggle.disabled,
    true,
    "The Override column menu item is disabled"
  );

  await hideContextMenu(overrideColumnToggle.parentNode);
}

/* exported assertOverrideCellStatus */
/**
 * Check if the provided cell is displayed as overridden or not.
 *
 * @param {object} request
 *     The request to assert.
 * @param {object} options
 * @param {boolean} options.overridden
 *     Whether the request is expected to be overridden or not.
 */
function assertOverrideCellStatus(request, { overridden }) {
  is(
    request
      .querySelector(".requests-list-override")
      .classList.contains("request-override-enabled"),
    overridden,
    `The request is ${overridden ? " " : "not "}shown as overridden`
  );
}

/**
 * Open the netmonitor context menu on the provided element
 *
 * @param {Object} monitor
 *        The network monitor object
 * @param {Element} el
 *        The element on which the menu should be opened
 * @param {String} id
 *        The id of the context menu item
 */
async function openContextMenuForItem(monitor, el, id) {
  EventUtils.sendMouseEvent({ type: "contextmenu" }, el);
  const menuItem = getContextMenuItem(monitor, id);
  const popup = menuItem.parentNode;

  if (popup.state != "open") {
    await BrowserTestUtils.waitForEvent(popup, "popupshown");
  }

  return menuItem;
}

/* exported setNetworkOverride */
/**
 * Setup a network override for the provided monitor, request, override file
 * name and content.
 *
 * @param {object} monitor
 *     The netmonitor monitor instance.
 * @param {object} request
 *     The request to override.
 * @param {string} overrideFileName
 *     The file name to use for the override.
 * @param {string} overrideContent
 *     The content to use for the override.
 * @returns {string}
 *     The path to the overridden file.
 */
async function setNetworkOverride(
  monitor,
  request,
  overrideFileName,
  overrideContent
) {
  const overridePath = prepareFilePicker(
    overrideFileName,
    monitor.toolbox.topWindow
  );

  info("Select the request to update");
  EventUtils.sendMouseEvent({ type: "mousedown" }, request);

  info("Use set override from the context menu");
  EventUtils.sendMouseEvent({ type: "contextmenu" }, request);
  const waitForSetOverride = waitForDispatch(
    monitor.toolbox.store,
    "SET_NETWORK_OVERRIDE"
  );
  await selectContextMenuItem(monitor, "request-list-context-set-override");
  await waitForSetOverride;

  info(`Wait for ${overrideFileName} to be saved to disk and re-write it`);
  await writeTextContentToPath(overrideContent, overridePath);

  return overridePath;
}

/* exported removeNetworkOverride */
/**
 * Remove a network override for the provided monitor and request.
 *
 * @param {object} monitor
 *     The netmonitor monitor instance.
 * @param {object} request
 *     The request for which the override should be removed.
 */
async function removeNetworkOverride(monitor, request) {
  info("Select the request to update");
  EventUtils.sendMouseEvent({ type: "mousedown" }, request);

  info("Use remove override from the context menu");
  EventUtils.sendMouseEvent({ type: "contextmenu" }, request);
  const waitForSetOverride = waitForDispatch(
    monitor.toolbox.store,
    "REMOVE_NETWORK_OVERRIDE"
  );
  await selectContextMenuItem(monitor, "request-list-context-remove-override");
  await waitForSetOverride;
}

/* exported prepareFilePicker */
/**
 * Mock the file picker.
 *
 * @param {string} filename
 *     The name of the file to create.
 * @param {XULWindow} chromeWindow
 *     The browser window.
 * @returns {string}
 *     The path of the mocked file.
 */
function prepareFilePicker(filename, chromeWindow) {
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(chromeWindow.browsingContext);
  const nsiFile = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, filename)
  );
  MockFilePicker.setFiles([nsiFile]);
  return nsiFile.path;
}

/* exported writeTextContentToPath */
/**
 * Update the text content of the file at the provided path.
 *
 * @param {string} textContent
 *     The text content to set.
 * @param {string} path
 *     The path of the file to update.
 */
async function writeTextContentToPath(textContent, path) {
  await BrowserTestUtils.waitForCondition(() => IOUtils.exists(path));
  await BrowserTestUtils.waitForCondition(async () => {
    const { size } = await IOUtils.stat(path);
    return size > 0;
  });

  // Bug 1946642: need to cleanup MockFilePicker before potentially creating
  // another one. Can't use registerCleanupFunction because it will only run
  // at the end of a full test, at this point init() might already have been
  // called more than once. Here the MockFilePicker has done its job and can
  // be cleaned up.
  SpecialPowers.MockFilePicker.cleanup();

  await IOUtils.write(path, new TextEncoder().encode(textContent));
}

/* exported setupNetworkOverridesTest */
/**
 * Sets up a basic server for network overrides tests, adds a tab loading this
 * server and starts the netmonitor.
 *
 * @returns {object}
 *     An object with monitor, tab and document properties.
 */
async function setupNetworkOverridesTest() {
  const baseURL = startOverridesHTTPServer();
  const TEST_URL = baseURL + "index.html";

  const { monitor, tab } = await initNetMonitor(TEST_URL, {
    requestCount: 3,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Reload to have 3 requests in the list
  const waitForEvents = waitForNetworkEvents(monitor, 3);
  await navigateTo(TEST_URL);
  await waitForEvents;

  return { monitor, tab, document };
}
