/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Adapted from devtools/client/performance-new/test/browser/helpers.js
function waitForPanelEvent(document, eventName) {
  return BrowserTestUtils.waitForEvent(document, eventName, false, event => {
    if (event.target.getAttribute("viewId") === "PanelUI-ipprotection") {
      return true;
    }
    return false;
  });
}
/* exported waitForPanelEvent */
