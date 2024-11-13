/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// TODO (bug 1928538): figure out what condition to wait for instead of setting
// an arbitrary timeout.
async function waitForUIUpdate() {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 500));
}

add_task(async function test_pref_toggles_menu() {
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
  // The pref observer only fires on change, and browser.toml sets the pref to
  // true, so we disable then re-enable the pref.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.enabled", false]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.enabled", true]],
  });
  await waitForUIUpdate();
  let menu = document.getElementById("profiles-menu");
  Assert.equal(menu.hidden, false, "menu should be visible when preffed on");
  await SpecialPowers.popPrefEnv();
  await waitForUIUpdate();
  Assert.equal(menu.hidden, true, "menu should be hidden when preffed off");
});

add_task(async function test_menu_contents_no_profiles() {
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.enabled", true]],
  });
  let popup = document.getElementById("menu_ProfilesPopup");

  // Uninit the service to simulate a user with no profiles.
  SelectableProfileService.uninit();
  await waitForUIUpdate();

  // Simulate opening the menu, as seen in browser_file_close_tabs.js.
  let updated = new Promise(resolve => {
    popup.addEventListener("popupshown", resolve, { once: true });
  });
  popup.dispatchEvent(new MouseEvent("popupshowing", { bubbles: true }));
  popup.dispatchEvent(new MouseEvent("popupshown", { bubbles: true }));
  await updated;

  let newProfileMenuItem = popup.querySelector("#menu_newProfile");
  ok(!!newProfileMenuItem, "should be a 'new profile' menu item");
  let manageProfilesMenuItem = popup.querySelector("#menu_manageProfiles");
  ok(!!manageProfilesMenuItem, "should be a 'manage profiles' menu item");
  let profileMenuItems = popup.querySelectorAll("menuitem[profileid]");
  Assert.equal(
    profileMenuItems.length,
    0,
    "should not be any profile items in the menu"
  );
});
