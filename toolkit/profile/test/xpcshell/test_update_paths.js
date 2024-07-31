/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests updating a profile's path on disk by setting its rootDir to another
 * profile directory, including the case where the profile is the dedicated
 * default for the current install.
 */
add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async () => {
    let hash = xreDirProvider.getInstallHash();
    let defaultProfile = makeRandomProfileDir("default");
    let updatedProfile = makeRandomProfileDir("updated");
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

    // Ensure the database and local data are consistent at the outset.
    checkProfileService(profilesIni);

    // Update the rootDir and flush changes to disk.
    let { profile } = selectStartupProfile();
    profile.rootDir = updatedProfile;
    let service = getProfileService();
    service.flush();

    // These are the database changes we expected.
    profilesIni.profiles[0].path = updatedProfile.leafName;
    profilesIni.installs[hash].default = updatedProfile.leafName;

    // Verify expected changes were made to the database.
    checkProfileService(profilesIni, false);

    // Finally, verify the profile object was updated as expected, including
    // updating the localDir (which is not saved to the database).
    Assert.ok(
      profile.rootDir.equals(updatedProfile),
      "rootDir should have been updated."
    );
    let expectedLocalDir = gDataHomeLocal.clone();
    expectedLocalDir.append(updatedProfile.leafName);
    Assert.ok(
      profile.localDir.equals(expectedLocalDir),
      "localDir should have been updated."
    );
  }
);
