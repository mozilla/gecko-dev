/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that warning messages can be grouped, per navigation and category, and that
// interacting with these groups works as expected.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";
const TEST_URI = "https://example.org/" + TEST_FILE;

const TRACKER_URL = "https://tracking.example.com/";
const FILE_PATH =
  "browser/devtools/client/webconsole/test/browser/test-image.png";
const CONTENT_BLOCKED_BY_ETP_URL = TRACKER_URL + FILE_PATH;

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

  info(
    "Log a tracking protection message to check a single message isn't grouped"
  );
  let onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  let { node } = await onContentBlockedByETPWarningMessage;
  is(
    node.querySelector(".warning-indent"),
    null,
    "The message has the expected style"
  );
  is(
    node.getAttribute("data-indent"),
    "0",
    "The message has the expected indent"
  );

  info("Log a simple message");
  await logString(hud, "simple message 1");

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  let onContentBlockedByETPWarningGroupMessage = waitForMessageByType(
    hud,
    ENHANCED_TRACKING_PROTECTION_GROUP_LABEL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  ({ node } = await onContentBlockedByETPWarningGroupMessage);
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 1`,
  ]);

  info("Log another simple message");
  await logString(hud, "simple message 2");

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info(
    "Log a third tracking protection message to check that the badge updates"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await waitFor(
    () => node.querySelector(".warning-group-badge").textContent == "3"
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findWarningMessage(hud, CONTENT_BLOCKED_BY_ETP_URL));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info(
    "Log a new tracking protection message to check it appears inside the group"
  );
  onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await onContentBlockedByETPWarningMessage;
  ok(true, "The new tracking protection message is displayed");

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info("Reload the page and wait for it to be ready");
  await reloadPage();

  // Also wait for the navigation message to be displayed.
  await waitFor(() =>
    findMessageByType(hud, "Navigated to", ".navigationMarker")
  );

  info("Log a tracking protection message to check it is not grouped");
  onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await onContentBlockedByETPWarningMessage;

  await logString(hud, "simple message 3");

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    "Navigated to",
    `${CONTENT_BLOCKED_BY_ETP_URL}?5`,
    `simple message 3`,
  ]);

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  onContentBlockedByETPWarningGroupMessage = waitForMessageByType(
    hud,
    ENHANCED_TRACKING_PROTECTION_GROUP_LABEL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  ({ node } = await onContentBlockedByETPWarningGroupMessage);
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info("Check that opening this group works");
  node.querySelector(".arrow").click();
  await waitFor(
    () => findWarningMessages(hud, CONTENT_BLOCKED_BY_ETP_URL).length === 6
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?5`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?6`,
    `simple message 3`,
  ]);

  info("Check that closing this group works, and let the other one open");
  node.querySelector(".arrow").click();
  await waitFor(
    () => findWarningMessages(hud, CONTENT_BLOCKED_BY_ETP_URL).length === 4
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info(
    "Log a third tracking protection message to check that the badge updates"
  );
  emitEnhancedTrackingProtectionMessage(hud);
  await waitFor(
    () => node.querySelector(".warning-group-badge").textContent == "3"
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?3`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `simple message 3`,
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
 * Log a string from the content page.
 *
 * @param {WebConsole} hud
 * @param {String} str
 */
function logString(hud, str) {
  const onMessage = waitForMessageByType(hud, str, ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [str], function (arg) {
    content.console.log(arg);
  });
  return onMessage;
}
