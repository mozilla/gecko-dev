/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that asyncFlushCurrentProfile succeeds if we startup into
 * the default managed profile for a profile group (see bug 1963173).
 */
add_task(
  {
    skip_if: () => !AppConstants.MOZ_SELECTABLE_PROFILES,
  },
  async () => {
    let hash = xreDirProvider.getInstallHash();
    let defaultProfile = makeRandomProfileDir("default");
    let otherProfile = makeRandomProfileDir("other");
    let storeID = "b0bacafe";
    let profilesIni = {
      profiles: [
        {
          name: "default",
          path: defaultProfile.leafName,
          storeID,
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

    Services.prefs.setCharPref("toolkit.profiles.storeID", storeID);

    let service = getProfileService();
    selectStartupProfile();

    // Overwrite profiles.ini: simulate another instance launching, getting
    // app focus, and flushing to disk, overwriting the default path.
    let overwriteProfilesIni = () => {
      let updated = {
        profiles: [
          {
            name: "default",
            path: otherProfile.leafName,
            storeID,
            default: true,
          },
        ],
        installs: {
          [hash]: {
            default: otherProfile.leafName,
          },
        },
      };
      writeProfilesIni(updated);
      let profileData = readProfilesIni();
      Assert.equal(
        profileData.profiles[0].path,
        otherProfile.leafName,
        "Default path should now be the unmanaged profile path"
      );
    };
    overwriteProfilesIni();

    // Now, simulate the default profile receiving app focus: asyncFlush would
    // fail, since profiles.ini has been updated since startup, but we should
    // then fall back to asyncFlushCurrentProfile, which should succeed.
    let asyncRewriteDefault = async () => {
      await service.asyncFlushCurrentProfile();
      let profileData = readProfilesIni();

      Assert.equal(
        profileData.profiles[0].path,
        defaultProfile.leafName,
        "AsyncFlushCurrentProfile should have updated the path to the path of the current managed profile"
      );
    };
    await asyncRewriteDefault();

    // Just to be sure, repeat the other instance setting itself to default,
    // then this instance flushing over top of those changes.
    overwriteProfilesIni();
    await asyncRewriteDefault();
  }
);
