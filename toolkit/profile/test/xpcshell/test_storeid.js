/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that the StoreID can be added and removed from profiles.ini.
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

    // Test that we can persist the StoreID to disk.
    let { profile } = selectStartupProfile();
    profile.storeID = "12345678";
    service.flush();
    profilesIni.profiles[0].storeID = "12345678";
    checkProfileService(profilesIni);

    // Next, test that we can remove the StoreID.
    profile.storeID = null;
    service.flush();
    profilesIni.profiles[0].storeID = null;
    checkProfileService(profilesIni);
  }
);
