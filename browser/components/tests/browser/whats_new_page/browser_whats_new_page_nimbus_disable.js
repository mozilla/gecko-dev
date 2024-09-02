/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function nimbus_whats_new_page_disable() {
  // The test harness will use the current tab and remove the tab's history.
  gBrowser.selectedTab = gBrowser.tabs[0];
  await TestUtils.waitForCondition(
    () =>
      gBrowser.selectedBrowser &&
      gBrowser.selectedBrowser.currentURI &&
      gBrowser.selectedBrowser.currentURI.spec == "about:blank",
    `Expected about:blank but got ${gBrowser.selectedBrowser.currentURI.spec}`
  );
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:blank",
    "What's New pages should be disabled"
  );

  let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  await TestUtils.waitForCondition(
    async () => !(await um.getReadyUpdate()),
    "Waiting for the ready update to be removed"
  );
  ok(!(await um.getReadyUpdate()), "There should not be a ready update");
  let history;
  await TestUtils.waitForCondition(async () => {
    history = await um.getHistory();
    return !!history[0];
  }, "Waiting for the ready update to be moved to the update history");
  ok(!!history[0], "There should be an update in the update history");

  // Leave no trace. Since this test modifies its support files put them back in
  // their original state.
  let alternatePath = Services.prefs.getCharPref("app.update.altUpdateDirPath");
  let testRoot = Services.prefs.getCharPref("mochitest.testRoot");
  let relativePath = alternatePath.substring("<test-root>".length);
  if (AppConstants.platform == "win") {
    relativePath = relativePath.replace(/\//g, "\\");
  }
  alternatePath = testRoot + relativePath;
  let updateDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  updateDir.initWithPath(alternatePath);

  let activeUpdateFile = updateDir.clone();
  activeUpdateFile.append("active-update.xml");
  await TestUtils.waitForCondition(
    () => !activeUpdateFile.exists(),
    "Waiting until the active-update.xml file does not exist"
  );

  let updatesFile = updateDir.clone();
  updatesFile.append("updates.xml");
  await TestUtils.waitForCondition(
    () => updatesFile.exists(),
    "Waiting until the updates.xml file exists"
  );

  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags =
    FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE;

  let stateSucceeded = "succeeded\n";
  let updateStatusFile = updateDir.clone();
  updateStatusFile.append("updates");
  updateStatusFile.append("0");
  updateStatusFile.append("update.status");
  updateStatusFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  fos.init(updateStatusFile, flags, FileUtils.PERMS_FILE, 0);
  fos.write(stateSucceeded, stateSucceeded.length);
  fos.close();

  let xmlContents =
    '<?xml version="1.0"?><updates xmlns="http://www.mozilla.org/2005/' +
    'app-update"><update xmlns="http://www.mozilla.org/2005/app-update" ' +
    'appVersion="61.0" buildID="20990101111111" channel="test" ' +
    'detailsURL="https://127.0.0.1/" displayVersion="1.0" installDate="' +
    '1555716429454" isCompleteUpdate="true" name="What\'s New Page Test" ' +
    'previousAppVersion="60.0" serviceURL="https://127.0.0.1/update.xml" ' +
    'type="minor" platformVersion="99999999.0" actions="showURL" ' +
    'openURL="https://example.com/|https://example.com/"><patch size="1" ' +
    'type="complete" URL="https://127.0.0.1/complete.mar" ' +
    'selected="true" state="pending"/></update></updates>\n';
  activeUpdateFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  fos.init(activeUpdateFile, flags, FileUtils.PERMS_FILE, 0);
  fos.write(xmlContents, xmlContents.length);
  fos.close();

  updatesFile.remove(false);
  await Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .internal.reload(false);
});
