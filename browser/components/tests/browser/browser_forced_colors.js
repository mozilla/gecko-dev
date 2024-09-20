/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_forced_colors_windows() {
  await SpecialPowers.pushPrefEnv({
    set: [["ui.useAccessibilityTheme", 1]],
  });

  ok(
    !matchMedia("(forced-colors: active)").matches,
    "forced-colors: active shouldn't match in chrome"
  );
  is(
    matchMedia("(forced-colors)").matches,
    AppConstants.platform == "win",
    "forced-colors should match with HCM enabled on windows, even in chrome"
  );
});
