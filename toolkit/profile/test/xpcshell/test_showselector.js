/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the boolean ShowSelector attribute can be set in profiles.ini
 * by setting the corresponding `showProfileSelector` attribute on a toolkit
 * profile object. Also tests that StoreID must be nonempty to set
 * ShowSelector, and deleting StoreID also deletes ShowSelector.
 */
add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async () => {
    let hash = xreDirProvider.getInstallHash();
    let defaultProfile = makeRandomProfileDir("default");
    let profilesIni = {
      profiles: [
        {
          name: "default",
          path: defaultProfile.leafName,
          default: true,
        },
      ],
      installs: {
        [hash]: {
          default: defaultProfile.leafName,
        },
      },
    };
    writeProfilesIni(profilesIni);
    let service = getProfileService();

    // Before testing, verify the service is consistent with the input.
    checkProfileService(profilesIni);

    // Test that an error is thrown if we try to set ShowSelector when there is
    // no StoreID.
    let { profile } = selectStartupProfile();
    Assert.throws(
      () => (profile.showProfileSelector = true),
      e => e.result == Cr.NS_ERROR_FAILURE,
      "Cannot set ShowSelector if StoreID is missing"
    );

    // Now, set a StoreID and test that we can set ShowSelector.
    profile.storeID = "12345678";
    profile.showProfileSelector = true;
    service.flush();

    // Expected value of the database.
    profilesIni.profiles[0].storeID = "12345678";
    profilesIni.profiles[0].showSelector = "1";

    // Check expected against actual database state.
    checkProfileService(profilesIni);

    // Next, flip the value to false, and verify again.
    profile.showProfileSelector = false;
    service.flush();
    profilesIni.profiles[0].showSelector = "0";
    checkProfileService(profilesIni);

    // Finally, verify that clearing storeID also clears ShowSelector.
    profile.showProfileSelector = true;
    profile.storeID = null;
    service.flush();
    delete profilesIni.profiles[0].storeID;
    delete profilesIni.profiles[0].showSelector;
    checkProfileService(profilesIni);
  }
);
