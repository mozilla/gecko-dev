/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs"
);

// Re-launches the current instance which will send arguments to the current
// instance via the remote system/
async function execRemote(args) {
  let process = await Subprocess.call({
    command: Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
    arguments: ["--profile", PathUtils.profileDir, ...args],
  });
  let { exitCode } = await process.wait();

  Assert.equal(exitCode, 0, "Should have exited normally");
}

add_task(async function test() {
  // A URL should open a new tab in the current window.
  let newTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.org/",
    true
  );
  await execRemote(["https://example.org/"]);
  Assert.ok(true, "Successfully sent remote message");
  let newTab = await newTabOpened;
  BrowserTestUtils.removeTab(newTab);

  // A blank command line should open a new window.
  let newWinOpened = BrowserTestUtils.waitForNewWindow();
  await execRemote([]);
  Assert.ok(true, "Successfully sent remote message");
  let newWindow = await newWinOpened;
  await BrowserTestUtils.closeWindow(newWindow);
});
