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

  // Simulate opening the menu.
  let updated = new Promise(resolve => {
    popup.addEventListener(
      "popupshown",
      async () => {
        await waitForUIUpdate();
        resolve();
      },
      { once: true }
    );
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

add_task(async function test_current_profile_marked() {
  registerCleanupFunction(async () => {
    await SelectableProfileService.uninit();
    await SpecialPowers.popPrefEnv();
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.enabled", true]],
  });

  await initGroupDatabase();
  await SelectableProfileService.init();

  let currentProfile = SelectableProfileService.currentProfile;
  Assert.ok(currentProfile, "Should have a current profile");

  currentProfile.name = "Current Profile";
  await SelectableProfileService.updateProfile(currentProfile);

  let profileData1 = {
    name: "Profile 1",
    avatar: "book",
    themeId: "firefox-compact-light@mozilla.org",
    themeFg: "rgb(21,20,26)",
    themeBg: "rgb(240,240,244)",
    path: "profile1",
  };
  await SelectableProfileService.insertProfile(profileData1);

  let profileData2 = {
    name: "Profile 2",
    avatar: "briefcase",
    themeId: "firefox-compact-dark@mozilla.org",
    themeFg: "rgb(255,255,255)",
    themeBg: "rgb(28,27,34)",
    path: "profile2",
  };
  await SelectableProfileService.insertProfile(profileData2);

  await waitForUIUpdate();

  let popup = document.getElementById("menu_ProfilesPopup");

  let updated = new Promise(resolve => {
    popup.addEventListener(
      "popupshown",
      async () => {
        await waitForUIUpdate();
        resolve();
      },
      { once: true }
    );
  });
  popup.dispatchEvent(new MouseEvent("popupshowing", { bubbles: true }));
  popup.dispatchEvent(new MouseEvent("popupshown", { bubbles: true }));
  await updated;

  let profileMenuItems = popup.querySelectorAll("menuitem[profileid]");
  Assert.equal(
    profileMenuItems.length,
    3,
    "should have 3 profile items in the menu"
  );

  // find which profile is marked as current by checking for the current class
  let currentMenuItem = null;
  for (let menuitem of profileMenuItems) {
    if (menuitem.classList.contains("current")) {
      currentMenuItem = menuitem;
      break;
    }
  }

  Assert.ok(currentMenuItem, "There should be a current profile marked");

  for (let menuitem of profileMenuItems) {
    if (menuitem === currentMenuItem) {
      Assert.ok(
        menuitem.classList.contains("current"),
        "current profile should have 'current' class"
      );
      Assert.equal(
        menuitem.getAttribute("data-l10n-id"),
        "menu-profiles-current",
        "current profile should have correct l10n id"
      );

      let l10nArgs = JSON.parse(menuitem.getAttribute("data-l10n-args"));
      Assert.ok(
        l10nArgs.profileName,
        "current profile should have a profile name in l10n args"
      );

      Assert.ok(
        menuitem.hasAttribute("label"),
        "current profile should have a label attribute"
      );
    } else {
      // non-current profiles should just have a label
      Assert.ok(
        !menuitem.classList.contains("current"),
        "non-current profile should not have 'current' class"
      );
      Assert.ok(
        !menuitem.hasAttribute("data-l10n-id"),
        "non-current profile should not have l10n id"
      );
      Assert.ok(
        !menuitem.hasAttribute("data-l10n-args"),
        "non-current profile should not have l10n args"
      );

      Assert.ok(
        menuitem.hasAttribute("label"),
        "non-current profile should have a label"
      );

      let label = menuitem.getAttribute("label");
      Assert.ok(
        label === "Profile 1" || label === "Profile 2",
        `non-current profile should have expected label, got: ${label}`
      );
    }
  }
});
