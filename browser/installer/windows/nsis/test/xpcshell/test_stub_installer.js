/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Subprocess } = ChromeUtils.importESModule(
  "resource://gre/modules/Subprocess.sys.mjs"
);

function _getBinaryUtil(binaryUtilName) {
  let utilBin = Services.dirsvc.get("GreD", Ci.nsIFile);
  // On macOS, GreD is .../Contents/Resources, and most binary utilities
  // are located there, but certutil is in GreBinD (or .../Contents/MacOS),
  // so we have to change the path accordingly.
  if (binaryUtilName === "certutil") {
    utilBin = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  }
  utilBin.append(binaryUtilName + mozinfo.bin_suffix);
  // If we're testing locally, the above works. If not, the server executable
  // is in another location.
  if (!utilBin.exists()) {
    utilBin = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    while (utilBin.path.includes("xpcshell")) {
      utilBin = utilBin.parent;
    }
    utilBin.append("bin");
    utilBin.append(binaryUtilName + mozinfo.bin_suffix);
  }
  // But maybe we're on Android, where binaries are in /data/local/xpcb.
  if (!utilBin.exists()) {
    utilBin.initWithPath("/data/local/xpcb/");
    utilBin.append(binaryUtilName);
  }
  Assert.ok(utilBin.exists(), `Binary util ${binaryUtilName} should exist`);
  return utilBin;
}

add_task(async function test_openFile() {
  // "GreD" is the "Gecko runtime environment directory", which the build system knows as $(topobjdir)/dist/bin
  const executableFile = _getBinaryUtil("test_stub_installer");
  const command = executableFile.path;

  const proc = await Subprocess.call({
    command,
  });

  let { exitCode } = await proc.wait();
  Assert.equal(0, exitCode);
  let stdout = await proc.stdout.readString();
  // Verify that the contents of the output file look OK.
  Assert.equal("All stub installer tests passed", stdout);
});
