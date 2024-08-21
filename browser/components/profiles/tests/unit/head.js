/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { SelectableProfile } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfile.sys.mjs"
);

const storeID = "12345678";
var gFakeAppDirectoryProvider;

function makeFakeProfileDirs() {
  do_get_profile();

  let dirMode = 0o700;
  let baseFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let appD = baseFile.clone();
  appD.append("UAppData");

  if (gFakeAppDirectoryProvider) {
    return Promise.resolve(appD.path);
  }

  function makeDir(f) {
    if (f.exists()) {
      return;
    }

    dump("Creating directory: " + f.path + "\n");
    f.create(Ci.nsIFile.DIRECTORY_TYPE, dirMode);
  }

  makeDir(appD);

  let profileDir = appD.clone();
  profileDir.append("Profiles");
  makeDir(profileDir);

  let defProfRtDir = appD.clone();
  defProfRtDir.append("DefProfRt");
  makeDir(defProfRtDir);

  let defProfLRtDir = appD.clone();
  defProfLRtDir.append("DefProfLRt");
  makeDir(defProfLRtDir);

  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop === "UAppData") {
        return appD.clone();
      } else if (prop === "ProfD") {
        return profileDir.clone();
      } else if (prop === "DefProfRt") {
        return defProfRtDir.clone();
      } else if (prop === "DefProfLRt") {
        return defProfLRtDir.clone();
      }

      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    },

    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };

  // Register the new provider.
  Services.dirsvc.registerProvider(provider);

  // And undefine the old one.
  try {
    Services.dirsvc.undefine("UAppData");
  } catch (ex) {}

  gFakeAppDirectoryProvider = provider;

  dump("Successfully installed fake UAppDir\n");
  return appD.path;
}

async function setupMockDB() {
  makeFakeProfileDirs();

  await IOUtils.createUniqueFile(
    PathUtils.join(
      Services.dirsvc.get("UAppData", Ci.nsIFile).path,
      "Profile Groups"
    ),
    `${storeID}.sqlite`
  );
}

add_setup(async () => {
  await setupMockDB();
});
