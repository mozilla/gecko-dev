/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

add_task(async function test_isolated_process_app_constant_defined() {
  Assert.notStrictEqual(
    typeof AppConstants.MOZ_ANDROID_CONTENT_SERVICE_ISOLATED_PROCESS,
    "undefined",
    "MOZ_ANDROID_CONTENT_SERVICE_ISOLATED_PROCESS is defined."
  );
});
