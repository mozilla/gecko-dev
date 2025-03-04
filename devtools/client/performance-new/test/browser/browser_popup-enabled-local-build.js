/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info(
    "Test that the profiler button gets automatically added only on non-official Mozilla builds."
  );

  const { ProfilerMenuButton } = ChromeUtils.importESModule(
    "resource://devtools/client/performance-new/popup/menu-button.sys.mjs"
  );

  if (AppConstants.MOZILLA_OFFICIAL) {
    ok(
      !ProfilerMenuButton.isInNavbar(),
      "The profiler popup is not enabled by default on official mozilla builds."
    );
  } else {
    ok(
      ProfilerMenuButton.isInNavbar(),
      "The profiler popup is enabled by default on local builds"
    );
    await waitUntil(
      () => document.getElementById("profiler-button"),
      "The profiler button was added to the browser."
    );
  }
});
