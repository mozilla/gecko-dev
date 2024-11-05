/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function resetPrefs() {
  Services.prefs.clearUserPref("browser.profiles.enabled");
}

// TODO (bug 1928538): figure out what condition to wait for instead of setting
// an arbitrary timeout.
async function waitForUIUpdate() {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 50));
}

add_task(async function test_pref_toggles_menu() {
  registerCleanupFunction(resetPrefs);
  let menu = document.getElementById("profiles-menu");

  Services.prefs.setBoolPref("browser.profiles.enabled", false);
  await waitForUIUpdate();
  Assert.equal(menu.hidden, true, "menu should be hidden when preffed off");

  Services.prefs.setBoolPref("browser.profiles.enabled", true);
  await waitForUIUpdate();
  Assert.equal(menu.hidden, false, "menu should be visible when preffed on");
});

add_task(async function test_menu_contents_no_profiles() {
  registerCleanupFunction(resetPrefs);
  Services.prefs.setBoolPref("browser.profiles.enabled", true);
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
