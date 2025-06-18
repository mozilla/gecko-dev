/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
async function test_verifyDefault() {
  // The current default for all platforms is to use "Single" directory layout.
  const install_dir_layout = Cc["@mozilla.org/install_dir_layout;1"].getService(Ci.nsIInstallationDirLayout);
  const isVersioned = install_dir_layout.isInstallationLayoutVersioned;
  Assert.equal(isVersioned, false, "Expected install_dir_layout.isInstallationLayoutVersioned to be false");
});
