/*
 * Bug 1330882 - A test case for opening new windows through window.open() as
 *   rounded size when fingerprinting resistance is enabled. This test is for
 *   maximum values.
 */

let targetWidth = Services.prefs.getIntPref("privacy.window.maxInnerWidth");
let targetHeight = Services.prefs.getIntPref("privacy.window.maxInnerHeight");

OpenTest.run([
  {
    settingWidth: targetWidth + 25,
    settingHeight: targetHeight + 50,
    targetWidth,
    targetHeight,
  },
  {
    settingWidth: 9999,
    settingHeight: 9999,
    targetWidth,
    targetHeight,
  },
  {
    settingWidth: targetWidth - 1,
    settingHeight: targetHeight - 1,
    targetWidth,
    targetHeight,
  },
]);
