/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cookie database schema 14
"use strict";

add_task(async function test_schema_14_migration() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies.sessionCookies;

  // Close the profile.
  await promise_close_profile();

  // Remove the cookie file in order to create another database file.
  do_get_cookie_file(profile).remove(false);

  // Create a schema 13 database.
  let schema13db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    13
  );

  let now = Math.round(Date.now() / 1000);

  let N = 20;
  // Populate db with N unexpired, unique cookies.
  for (let i = 0; i < N; ++i) {
    let cookie = new Cookie(
      "test" + i,
      "Some data",
      "foo.com",
      "/",
      now + (i % 2 ? 34560000 * 2 : 0),
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_UNSET,
      !!(i % 2) // isPartitioned
    );
    schema13db.insertCookie(cookie);
  }

  schema13db.close();
  schema13db = null;

  // Check if we have the right entries
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT count(name) FROM moz_cookies WHERE expiry > unixepoch() + 34560000"
    );
    const success = stmt.executeStep();
    Assert.ok(success);

    const count = stmt.getInt32(0);
    Assert.equal(count, 10);
    stmt.finalize();
    dbConnection.close();
  }

  // Reload profile.
  await promise_load_profile();

  // Assert inserted cookies are in the db and correctly handled by services.
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), N);

  // Check if the time was reset
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT COUNT(name) FROM moz_cookies WHERE expiry <= unixepoch() + 34560000"
    );
    const success = stmt.executeStep();
    Assert.ok(success);

    const count = stmt.getInt32(0);
    Assert.equal(count, 20);
    stmt.finalize();
    dbConnection.close();
  }

  // Cleanup
  Services.cookies.removeAll();
  do_close_profile();
});
