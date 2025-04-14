/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that warning groups are not created outside console.group.

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

// Tracking protection preferences
pushPref("privacy.trackingprotection.enabled", true);

const ENHANCED_TRACKING_PROTECTION_GROUP_LABEL =
  "The resource at “<URL>” was blocked because Enhanced Tracking Protection is enabled.";

add_task(async function testEnhancedTrackingProtectionMessage() {
  // Enable groupWarning and persist log
  await pushPref("devtools.webconsole.groupWarningMessages", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a console.group");
  const onGroupMessage = waitForMessageByType(hud, "myGroup", ".console-api");
  let onInGroupMessage = waitForMessageByType(
    hud,
    "log in group",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.group("myGroup");
    content.wrappedJSObject.console.log("log in group");
  });
  const { node: consoleGroupMessageNode } = await onGroupMessage;
  await onInGroupMessage;

  await checkConsoleOutputForWarningGroup(hud, [`▼ myGroup`, `| log in group`]);

  info(
    "Log a tracking protection message to check a single message isn't grouped"
  );
  const now = Date.now();
  let onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(now);
  await onContentBlockedByETPWarningMessage;

  await checkConsoleOutputForWarningGroup(hud, [
    `▼ myGroup`,
    `| log in group`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findConsoleAPIMessage(hud, "log in group"));

  await checkConsoleOutputForWarningGroup(hud, [`▶︎ myGroup`]);

  info("Expand the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => findConsoleAPIMessage(hud, "log in group"));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼ myGroup`,
    `| log in group`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
  ]);

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  const onContentBlockedByETPWarningGroupMessage = waitForMessageByType(
    hud,
    ENHANCED_TRACKING_PROTECTION_GROUP_LABEL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(now);
  const { node: warningGroupNode } =
    await onContentBlockedByETPWarningGroupMessage;

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info("Open the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findWarningMessage(hud, CONTENT_BLOCKED_BY_ETP_URL));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-2`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info(
    "Log a new tracking protection message to check it appears inside the group"
  );
  onContentBlockedByETPWarningMessage = waitForMessageByType(
    hud,
    CONTENT_BLOCKED_BY_ETP_URL,
    ".warn"
  );
  emitEnhancedTrackingProtectionMessage(now);
  await onContentBlockedByETPWarningMessage;
  ok(true, "The new tracking protection message is displayed");

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-3`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info("Log a simple message to check if it goes into the console.group");
  onInGroupMessage = waitForMessageByType(hud, "log in group", ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log("second log in group");
  });
  await onInGroupMessage;

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-3`,
    `▼ myGroup`,
    `| log in group`,
    `| second log in group`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findConsoleAPIMessage(hud, "log in group"));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-3`,
    `▶︎ myGroup`,
  ]);

  info("Close the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => !findWarningMessage(hud, CONTENT_BLOCKED_BY_ETP_URL));

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `▶︎ myGroup`,
  ]);

  info("Open the console group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => findConsoleAPIMessage(hud, "log in group"));

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `▼ myGroup`,
    `| log in group`,
    `| second log in group`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findConsoleAPIMessage(hud, "log in group"));

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `▶︎ myGroup`,
  ]);

  info("Open the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findWarningMessage(hud, CONTENT_BLOCKED_BY_ETP_URL));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${ENHANCED_TRACKING_PROTECTION_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-1`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-2`,
    `| ${CONTENT_BLOCKED_BY_ETP_URL}?${now}-3`,
    `▶︎ myGroup`,
  ]);
});

let cpt = 0;
/**
 * Emit an Enhanced Tracking Protection message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitEnhancedTrackingProtectionMessage(prefix) {
  const url = `${CONTENT_BLOCKED_BY_ETP_URL}?${prefix}-${++cpt}`;
  SpecialPowers.spawn(gBrowser.selectedBrowser, [url], function (innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}
