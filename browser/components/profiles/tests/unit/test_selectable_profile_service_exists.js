/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_SelectableProfileAndServiceExist() {
  const { SelectableProfile } = ChromeUtils.importESModule(
    "resource:///modules/profiles/SelectableProfile.sys.mjs"
  );
  const { SelectableProfileService } = ChromeUtils.importESModule(
    "resource:///modules/profiles/SelectableProfileService.sys.mjs"
  );

  ok(SelectableProfile, "SelectableProfile exists");
  ok(SelectableProfileService, "SelectableProfileService exists");
});
