/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EVENTS } = require("devtools/client/netmonitor/src/constants");
const {
  openToolbox,
  closeToolbox,
  reloadPageAndLog,
  testSetup,
  testTeardown,
  runTest,
  PAGES_BASE_URL,
} = require("damp-test/tests/head");

const {
  createContext,
  selectSource,
  waitUntil,
  waitForSources,
} = require("../debugger/debugger-helpers");

module.exports = async function () {
  await testSetup(PAGES_BASE_URL + "custom/panels-in-background/index.html");

  // Make sure the Console and Network panels are initialized
  let toolbox = await openToolbox("webconsole");
  let monitor = await toolbox.selectTool("netmonitor");
  const debuggerPanel = await toolbox.selectTool("jsdebugger");
  const dbg = await createContext(debuggerPanel);
  dump(" Select and pretty print a source");
  await selectSource(dbg, "eval-script-0");
  const prettyPrintButton = await waitUntil(() => {
    return dbg.win.document.querySelector(".source-footer .prettyPrint.active");
  });
  prettyPrintButton.click();

  // Select the options panel to make all the previously selected panel
  // be in background.
  // Options panel should not do anything on page reload.
  await toolbox.selectTool("options");

  // Reload the page and wait for all HTTP requests
  // to finish (1 doc + 600 XHRs).
  let test = runTest("panelsInBackground.redux-updates.DAMP");
  let payloadReady = waitForPayload(601, monitor.panelWin);
  await reloadPageAndLog("panelsInBackground", toolbox);
  await payloadReady;

  // Also wait for sources to be registered in the Debugger Redux store
  // The HTML page + 2000 eval sources
  await waitForSources(dbg, 2001);
  test.done();

  await closeToolbox();
  await testTeardown();
};

function waitForPayload(count, panelWin) {
  return new Promise(resolve => {
    let payloadReady = 0;

    function onPayloadReady() {
      payloadReady++;
      maybeResolve();
    }

    function maybeResolve() {
      if (payloadReady >= count) {
        panelWin.api.off(EVENTS.PAYLOAD_READY, onPayloadReady);
        resolve();
      }
    }

    panelWin.api.on(EVENTS.PAYLOAD_READY, onPayloadReady);
  });
}
