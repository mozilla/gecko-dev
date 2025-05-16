/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

do_get_profile();
