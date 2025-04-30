/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cookie database schema 15
"use strict";

add_task(async function test_schema_15_migration() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies.sessionCookies;

  // Close the profile.
  await promise_close_profile();

  // Remove the cookie file in order to create another database file.
  do_get_cookie_file(profile).remove(false);

  // Create a schema 14 database.
  let schema14db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    14
  );

  let now = Math.round(Date.now() / 1000);

  // This cookie will be updated from NONE/LAX to UNSET
  let cookie = new Cookie(
    "test0",
    "Some data",
    "foo.com",
    "/",
    now,
    now,
    now,
    false,
    false,
    false,
    false,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_UNSET
  );
  cookie.sameSite = Ci.nsICookie.SAMESITE_LAX;
  schema14db.insertCookie(cookie);

  schema14db.insertCookie(
    new Cookie(
      "test1",
      "Some data",
      "foo.com",
      "/",
      now,
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_UNSET,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema14db.insertCookie(
    new Cookie(
      "test2",
      "Some data",
      "foo.com",
      "/",
      now,
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_LAX,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema14db.insertCookie(
    new Cookie(
      "test3",
      "Some data",
      "foo.com",
      "/",
      now,
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_STRICT,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema14db.insertCookie(
    new Cookie(
      "test4",
      "Some data",
      "foo.com",
      "/",
      now,
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  // This cookie will be updated from NONE/LAX to UNSET
  cookie = new Cookie(
    "test5",
    "Some data",
    "foo.com",
    "/",
    now,
    now,
    now,
    false,
    false,
    false,
    false,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_UNSET
  );
  cookie.sameSite = Ci.nsICookie.SAMESITE_LAX;
  schema14db.insertCookie(cookie);

  schema14db.close();
  schema14db = null;

  // Check if we have the right entries
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite FROM moz_cookies"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({ name: stmt.getString(0), sameSite: stmt.getInt32(1) });
    }

    Assert.deepEqual(results, [
      { name: "test0", sameSite: Ci.nsICookie.SAMESITE_LAX },
      { name: "test1", sameSite: Ci.nsICookie.SAMESITE_UNSET },
      { name: "test2", sameSite: Ci.nsICookie.SAMESITE_LAX },
      { name: "test3", sameSite: Ci.nsICookie.SAMESITE_STRICT },
      { name: "test4", sameSite: Ci.nsICookie.SAMESITE_NONE },
      { name: "test5", sameSite: Ci.nsICookie.SAMESITE_LAX },
    ]);

    stmt.finalize();
    dbConnection.close();
  }

  // Reload profile.
  await promise_load_profile();

  // Assert inserted cookies are in the db and correctly handled by services.
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), 6);

  // Check if the time was reset
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite FROM moz_cookies"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({ name: stmt.getString(0), sameSite: stmt.getInt32(1) });
    }

    Assert.deepEqual(results, [
      { name: "test0", sameSite: Ci.nsICookie.SAMESITE_UNSET },
      { name: "test1", sameSite: Ci.nsICookie.SAMESITE_UNSET },
      { name: "test2", sameSite: Ci.nsICookie.SAMESITE_LAX },
      { name: "test3", sameSite: Ci.nsICookie.SAMESITE_STRICT },
      { name: "test4", sameSite: Ci.nsICookie.SAMESITE_NONE },
      { name: "test5", sameSite: Ci.nsICookie.SAMESITE_UNSET },
    ]);

    stmt.finalize();
    dbConnection.close();
  }

  // Cleanup
  Services.cookies.removeAll();
  do_close_profile();
});
