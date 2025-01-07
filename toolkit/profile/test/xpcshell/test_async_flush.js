/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Verifies the async flushing methods.
 */

add_task(async () => {
  let defaultProfile = makeRandomProfileDir("default");

  let hash = xreDirProvider.getInstallHash();

  writeCompatibilityIni(defaultProfile);

  writeProfilesIni({
    profiles: [
      {
        name: "default",
        path: defaultProfile.leafName,
        default: false,
      },
    ],
    installs: {
      [hash]: { default: defaultProfile.leafName },
    },
  });

  selectStartupProfile();
  checkStartupReason("default");

  let profileData = readProfilesIni();
  checkProfileService(profileData);

  let service = getProfileService();

  let newProfileDir = makeRandomProfileDir("newProfile");
  service.createProfile(newProfileDir, "new");

  await service.asyncFlush();
  profileData = readProfilesIni();

  Assert.equal(profileData.profiles.length, 2, "Should now have two profiles.");

  checkProfileService(profileData);

  let other1 = makeRandomProfileDir("other1");
  let other2 = makeRandomProfileDir("other2");

  // Write out a different ini file.
  writeProfilesIni({
    profiles: [
      {
        name: "changedname",
        path: defaultProfile.leafName,
      },
      {
        name: "other1",
        path: other1.leafName,
      },
      {
        name: "other2",
        path: other2.leafName,
      },
    ],
    installs: {
      [hash]: { default: defaultProfile.leafName },
    },
  });

  // Change the modified time.
  let profilesini = gDataHome.clone();
  profilesini.append("profiles.ini");
  let oldTime = profilesini.lastModifiedTime;
  profilesini.lastModifiedTime = oldTime - 10000;

  try {
    await service.asyncFlush();
    Assert.ok(false, "Flushing should have failed");
  } catch (e) {
    Assert.ok(true, "Flushing should have failed");
  }

  profileData = readProfilesIni();
  Assert.equal(profileData.profiles.length, 3, "Should have three profiles.");

  let found = profileData.profiles.find(p => p.name == "changedname");
  Assert.ok(found, "Should have found the current profile.");
  Assert.equal(found.path, defaultProfile.leafName);
  Assert.equal(found.storeID, null);

  found = profileData.profiles.find(p => p.name == "other1");
  Assert.ok(found, "Should have found the other1 profile.");
  Assert.equal(found.path, other1.leafName);
  Assert.equal(found.storeID, null);

  found = profileData.profiles.find(p => p.name == "other2");
  Assert.ok(found, "Should have found the other2 profile.");
  Assert.equal(found.path, other2.leafName);
  Assert.equal(found.storeID, null);

  let installData = readInstallsIni();
  Assert.equal(profileData.installs[hash].default, defaultProfile.leafName);
  Assert.equal(installData.installs[hash].default, defaultProfile.leafName);

  if (AppConstants.MOZ_SELECTABLE_PROFILES) {
    // Set a store ID on the profile. Flushing will succeed because the profile path hasn't changed.
    service.currentProfile.storeID = "7126354jdf";

    await service.asyncFlushGroupProfile();

    profileData = readProfilesIni();
    Assert.equal(profileData.profiles.length, 3, "Should have three profiles.");

    found = profileData.profiles.find(p => p.name == "changedname");
    Assert.ok(found, "Should have found the current profile.");
    Assert.equal(found.path, defaultProfile.leafName);
    Assert.equal(found.storeID, "7126354jdf");

    found = profileData.profiles.find(p => p.name == "other1");
    Assert.ok(found, "Should have found the other1 profile.");
    Assert.equal(found.path, other1.leafName);
    Assert.equal(found.storeID, null);

    found = profileData.profiles.find(p => p.name == "other2");
    Assert.ok(found, "Should have found the other2 profile.");
    Assert.equal(found.path, other2.leafName);
    Assert.equal(found.storeID, null);

    installData = readInstallsIni();
    Assert.equal(profileData.installs[hash].default, defaultProfile.leafName);
    Assert.equal(installData.installs[hash].default, defaultProfile.leafName);

    // Change the profile path. Flushing will succeed because the store ID now matches.
    service.currentProfile.rootDir = newProfileDir;

    await service.asyncFlushGroupProfile();

    profileData = readProfilesIni();
    Assert.equal(profileData.profiles.length, 3, "Should have three profiles.");

    found = profileData.profiles.find(p => p.name == "changedname");
    Assert.ok(found, "Should have found the current profile.");
    Assert.equal(found.path, newProfileDir.leafName);
    Assert.equal(found.storeID, "7126354jdf");

    found = profileData.profiles.find(p => p.name == "other1");
    Assert.ok(found, "Should have found the other1 profile.");
    Assert.equal(found.path, other1.leafName);
    Assert.equal(found.storeID, null);

    found = profileData.profiles.find(p => p.name == "other2");
    Assert.ok(found, "Should have found the other2 profile.");
    Assert.equal(found.path, other2.leafName);
    Assert.equal(found.storeID, null);

    installData = readInstallsIni();
    Assert.equal(profileData.installs[hash].default, newProfileDir.leafName);
    Assert.equal(installData.installs[hash].default, newProfileDir.leafName);

    // Modify the on-disk data
    writeProfilesIni({
      profiles: [
        {
          name: "some other name",
          path: "some other directory",
          storeID: "7126354jdf",
        },
      ],
      installs: {
        [hash]: { default: "some other directory" },
      },
    });

    await service.asyncFlushGroupProfile();

    profileData = readProfilesIni();
    Assert.equal(profileData.profiles.length, 1, "Should have one profile.");

    found = profileData.profiles[0];
    Assert.ok(found, "Should have found the current profile.");
    Assert.equal(found.path, newProfileDir.leafName);
    Assert.equal(found.storeID, "7126354jdf");
  }
});
