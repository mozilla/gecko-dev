/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selector_window() {
  if (!AppConstants.MOZ_SELECTABLE_PROFILES) {
    // `mochitest-browser` suite `add_task` does not yet support
    // `properties.skip_if`.
    ok(true, "Skipping because !AppConstants.MOZ_SELECTABLE_PROFILES");
    return;
  }

  await initGroupDatabase();

  let notifications = [];
  let observer = (subject, topic, data) => {
    notifications.push(data);
  };
  Services.obs.addObserver(observer, "sps-profiles-updated");
  registerCleanupFunction(() => {
    Services.obs.removeObserver(observer, "sps-profiles-updated");
  });

  // We inject an additional profile using the same profile path. This will be
  // the target of the notification but since it uses the same path it will
  // actually send the message to this running instance. But in order to do this
  // we must remove the UNIQUE constraint from the database...
  let connection = await openDatabase();
  // SQLite is a bit tiresome in that we can't alter existing columns but we can
  // somewhat fake it.
  await connection.executeTransaction(async () => {
    await connection.execute(`ALTER TABLE "Profiles" RENAME TO "old"`);
    await connection.execute(
      `CREATE TABLE IF NOT EXISTS "Profiles" (
         id  INTEGER NOT NULL,
         path	TEXT NOT NULL,
         name	TEXT NOT NULL,
         avatar	TEXT NOT NULL,
         themeId	TEXT NOT NULL,
         themeFg	TEXT NOT NULL,
         themeBg	TEXT NOT NULL,
         PRIMARY KEY(id)
      )`
    );
    await connection.execute(`INSERT INTO "Profiles" SELECT * FROM "old"`);
    await connection.execute(`DROP TABLE "old"`);
  });

  let profileData = {
    name: "New Profile",
    avatar: "book",
    themeId: "default",
    themeFg: "var(--text-color)",
    themeBg: "var(--background-color-box)",
    path: SelectableProfileService.getRelativeProfilePath(
      Services.dirsvc.get("ProfD", Ci.nsIFile)
    ),
  };

  let profile = await SelectableProfileService.insertProfile(profileData);

  // This will have caused notifications both to the current profile and the
  // "new" profile (though in reality the new profile will normally never be
  // running).
  await TestUtils.waitForCondition(() => notifications.length >= 2);

  Assert.equal(notifications[0], "local");
  Assert.equal(notifications[1], "remote");
  Assert.equal(notifications.length, 2);

  // Changing the name should notify.
  notifications = [];
  profile.name = "Foo";

  await TestUtils.waitForCondition(() => notifications.length >= 2);

  Assert.equal(notifications[0], "local");
  Assert.equal(notifications[1], "remote");
  Assert.equal(notifications.length, 2);

  // Setting shared prefs should notify.
  notifications = [];
  Services.prefs.setCharPref("test.pref1", "hello");
  await SelectableProfileService.trackPref("test.pref1");

  await TestUtils.waitForCondition(() => notifications.length >= 2);

  Assert.equal(notifications[0], "local");
  Assert.equal(notifications[1], "remote");
  // Notifications should be debounced
  Assert.equal(notifications.length, 2);

  // Properly simulate a set from another instance.
  notifications = [];
  await connection.execute(
    "INSERT INTO SharedPrefs(id, name, value, isBoolean) VALUES (NULL, :name, :value, :isBoolean) ON CONFLICT(name) DO UPDATE SET value=excluded.value, isBoolean=excluded.isBoolean;",
    {
      name: "test.pref4",
      value: "remoted",
      isBoolean: false,
    }
  );

  let remoteService = Cc["@mozilla.org/remote;1"].getService(
    Ci.nsIRemoteService
  );
  remoteService.sendCommandLine(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    ["--profiles-updated"],
    false
  );

  await TestUtils.waitForCondition(() => notifications.length >= 1);

  Assert.equal(notifications[0], "remote");
  Assert.equal(notifications.length, 1);

  Assert.equal(Services.prefs.getCharPref("test.pref4", null), "remoted");

  await connection.close();
});
