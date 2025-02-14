/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_check_from_os() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let zipType = mimeService.getTypeFromExtension("zip");
  Assert.equal(zipType, "application/zip");
  try {
    let extension = mimeService.getPrimaryExtension("application/zip", "");
    Assert.equal(
      extension,
      "zip",
      "Expect our own info to provide an extension for zip files."
    );
  } catch (ex) {
    Assert.ok(false, "We shouldn't throw when getting zip info.");
  }

  try {
    let found = {};
    mimeService.getMIMEInfoFromOS("application/zip", "zip", found);
    Assert.ok(found.value, "OS API should resolve zip mimetype and extension");
  } catch (ex) {
    Assert.ok(
      false,
      "Getting extension for 'application/zip' should not throw."
    );
  }

  try {
    let found = {};
    mimeService.getMIMEInfoFromOS("", "abc", found);
    Assert.ok(!found.value, "OS API shouldn't resolve unknown and extension");
  } catch (ex) {
    Assert.ok(false, "We shouldn't throw when getting unknown file info.");
  }
});
