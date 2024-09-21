/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { makeFakeAppDir } = ChromeUtils.importESModule(
  "resource://testing-common/AppData.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { SelectableProfile } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfile.sys.mjs"
);

const storeID = "12345678";
var gFakeAppDirectoryProvider;

function makeFakeProfileDirs() {
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

  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop === "UAppData") {
        return appD.clone();
      } else if (
        prop === "ProfD" ||
        prop === "DefProfRt" ||
        prop === "DefProfLRt"
      ) {
        return profileDir.clone();
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

function updateProfD(profileDir) {
  let profD = profileDir.clone();

  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop === "ProfD") {
        return profD.clone();
      }

      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    },

    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };

  // Register the new provider.
  Services.dirsvc.registerProvider(provider);

  // And undefine the old one.
  try {
    Services.dirsvc.undefine("ProfD");
  } catch (ex) {}
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

  // re-initialize because we updated the dirsvc
  await SelectableProfileService.uninit();
  await SelectableProfileService.init();

  let profile = await SelectableProfileService.createProfile({
    name: "testProfile",
    avatar: "avatar",
    themeL10nId: "theme-id",
    themeFg: "redFG",
    themeBg: "blueBG",
  });

  updateProfD(await profile.rootDir);

  return profile;
}
