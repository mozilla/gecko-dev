/*
 * Bug 1330882 - A test case for opening new windows through window.open() as
 *   rounded size when fingerprinting resistance is enabled. This test is for
 *   minimum values.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });
});

OpenTest.run([
  {
    settingWidth: 199,
    settingHeight: 99,
    targetWidth: 200,
    targetHeight: 100,
  },
  {
    settingWidth: 10,
    settingHeight: 10,
    targetWidth: 200,
    targetHeight: 100,
  },
]);
