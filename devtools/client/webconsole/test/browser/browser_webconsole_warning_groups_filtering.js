/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filtering the console output when there are warning groups works as expected.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";
const TEST_URI = "https://example.org/" + TEST_FILE;

const TRACKER_URL = "https://tracking.example.com/";
const IMG_FILE =
  "browser/devtools/client/webconsole/test/browser/test-image.png";
const CONTENT_BLOCKED_BY_ETP_URL = TRACKER_URL + IMG_FILE;

const { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);
UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function () {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

pushPref("privacy.trackingprotection.enabled", true);
pushPref("devtools.webconsole.groupWarningMessages", true);

const ENHANCED_TRACKING_PROTECTION_GROUP_LABEL =
  "The resource at “<URL>” was blocked because Enhanced Tracking Protection is enabled.";

add_task(async function testEnhancedTrackingProtectionMessage() {
  // Enable groupWarning and persist log
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a few tracking protection messages and simple ones");
  let onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await onContentBlockedByETPWarningMessage;
  await logStrings(hud, "simple message A");
  let onContentBlockedByETPWarningGroupMessage = waitForMessageByType(
    hud,
    ENHANCED_TRACKING_PROTECTION_GROUP_LABEL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  const warningGroupMessage1 = (await onContentBlockedByETPWarningGroupMessage)
    .node;
  await logStrings(hud, "simple message B");
  emitEnhancedTrackingProtectionMessage(hud);
  await waitForBadgeNumber(warningGroupMessage1, "3");
  emitEnhancedTrackingProtectionMessage(hud);
  await waitForBadgeNumber(warningGroupMessage1, "4");

  info("Reload the page and wait for it to be ready");
  await reloadPage();

  // Wait for the navigation message to be displayed.
  await waitFor(() =>
    findMessageByType(hud, "Navigated to", ".navigationMarker")
  );

  onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await onContentBlockedByETPWarningMessage;
  await logStrings(hud, "simple message C");
  onContentBlockedByETPWarningGroupMessage = waitForMessageByType(
    hud,
    ENHANCED_TRACKING_PROTECTION_GROUP_LABEL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  const warningGroupMessage2 = (await onContentBlockedByETPWarningGroupMessage)
    .node;
  emitEnhancedTrackingProtectionMessage(hud);
  await waitForBadgeNumber(warningGroupMessage2, "3");

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Filter warnings");
  await setFilterState(hud, { warn: false });
  await waitFor(
    () => !findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Display warning messages again");
  await setFilterState(hud, { warn: true });
  await waitFor(() =>
    findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Expand the first warning group");
  findWarningMessages(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)[0]
    .querySelector(".arrow")
    .click();
  await waitFor(() => findWarningMessage(hud, CONTENT_BLOCKED_BY_ETP_URL));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Filter warnings");
  await setFilterState(hud, { warn: false });
  await waitFor(
    () => !findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Display warning messages again");
  await setFilterState(hud, { warn: true });
  await waitFor(() =>
    findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Filter on warning group text");
  await setFilterState(hud, { text: ENHANCED_TRACKING_PROTECTION_GROUP_LABEL });
  await waitFor(() => !findConsoleAPIMessage(hud, "simple message"));
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
  ]);

  info("Open the second warning group");
  findWarningMessages(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)[1]
    .querySelector(".arrow")
    .click();
  await waitFor(() => findWarningMessage(hud, "?6"));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `Navigated to`,
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?5`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?6`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?7`,
  ]);

  info("Filter on warning message text from a single warning group");
  await setFilterState(hud, { text: "/\\?(2|4)/" });
  await waitFor(() => !findWarningMessage(hud, "?1"));
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `Navigated to`,
  ]);

  info("Filter on warning message text from two warning groups");
  await setFilterState(hud, { text: "/\\?(3|6|7)/" });
  await waitFor(() => findWarningMessage(hud, "?7"));
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `Navigated to`,
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?6`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?7`,
  ]);

  info("Clearing text filter");
  await setFilterState(hud, { text: "" });
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?5`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?6`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?7`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Filter warnings with two opened warning groups");
  await setFilterState(hud, { warn: false });
  await waitFor(
    () => !findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );
  await checkConsoleOutputForWarningGroup(hud, [
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `simple message C #1`,
    `simple message C #2`,
  ]);

  info("Display warning messages again with two opened warning groups");
  await setFilterState(hud, { warn: true });
  await waitFor(() =>
    findWarningMessage(hud, ENHANCED_TRACKING_PROTECTION_GROUP_LABEL)
  );
  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message A #1`,
    `simple message A #2`,
    `simple message B #1`,
    `simple message B #2`,
    `Navigated to`,
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?5`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?6`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?7`,
    `simple message C #1`,
    `simple message C #2`,
  ]);
});

let cpt = 0;
/**
 * Emit an Enhanced Tracking Protection message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitEnhancedTrackingProtectionMessage() {
  const url = `${CONTENT_BLOCKED_BY_ETP_URL}?${++cpt}`;
  SpecialPowers.spawn(gBrowser.selectedBrowser, [url], function (innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}

/**
 * Log 2 string messages from the content page. This is done in order to increase the
 * chance to have messages sharing the same timestamp (and making sure filtering and
 * ordering still works fine).
 *
 * @param {WebConsole} hud
 * @param {String} str
 */
function logStrings(hud, str) {
  const onFirstMessage = waitForMessageByType(hud, `${str} #1`, ".console-api");
  const onSecondMessage = waitForMessageByType(
    hud,
    `${str} #2`,
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [str], function (arg) {
    content.console.log(arg, "#1");
    content.console.log(arg, "#2");
  });
  return Promise.all([onFirstMessage, onSecondMessage]);
}

function waitForBadgeNumber(message, expectedNumber) {
  return waitFor(
    () =>
      message.querySelector(".warning-group-badge").textContent ==
      expectedNumber
  );
}
