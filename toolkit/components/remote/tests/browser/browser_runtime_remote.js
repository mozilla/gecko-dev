/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests using the runtime remote service to send remote messages to ourselves.
// Linux is not supported where the main thread needs to process events.
add_task(async function test() {
  let remoteService = Cc["@mozilla.org/remote;1"].getService(
    Ci.nsIRemoteService
  );

  // A URL should open a new tab in the current window.
  let newTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.org/",
    true
  );
  remoteService.sendCommandLine(PathUtils.profileDir, ["https://example.org/"]);
  Assert.ok(true, "Successfully sent remote message");
  let newTab = await newTabOpened;
  BrowserTestUtils.removeTab(newTab);

  // A blank command line should open a new window.
  let newWinOpened = BrowserTestUtils.waitForNewWindow();
  remoteService.sendCommandLine(PathUtils.profileDir, []);
  Assert.ok(true, "Successfully sent remote message");
  let newWindow = await newWinOpened;
  await BrowserTestUtils.closeWindow(newWindow);

  // A bad profile folder should throw.
  Assert.throws(
    () => {
      remoteService.sendCommandLine("invalid/path", []);
    },
    /NS_ERROR_NOT_AVAILABLE/,
    "Should throw for a missing instance"
  );
});
