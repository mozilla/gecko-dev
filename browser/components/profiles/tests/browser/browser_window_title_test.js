/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_windowTitle() {
  // The currentProfile is null, because there are 0 profiles in the db, when
  // updateTitlebar is called in the EveryWindow init function.
  // So we uninit and inti again so we have a current profile when
  // updateTitlebar is called.
  await initGroupDatabase();
  await SelectableProfileService.uninit();
  await SelectableProfileService.init();

  const profileName = SelectableProfileService.currentProfile.name;

  Assert.ok(
    document.title.includes(profileName),
    "The profile name is in the window title"
  );

  await SelectableProfileService.uninit();

  Assert.ok(
    !document.title.includes(profileName),
    "The profile name is not in the window title"
  );
});
