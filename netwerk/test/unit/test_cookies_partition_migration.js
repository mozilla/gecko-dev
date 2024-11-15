/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_chips_migration() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies.sessionCookies;

  // Close the profile.
  await promise_close_profile();

  // Remove the cookie file in order to create another database file.
  do_get_cookie_file(profile).remove(false);

  // Create a schema 14 database.
  let database = new CookieDatabaseConnection(do_get_cookie_file(profile), 14);

  let now = Date.now() * 1000;
  let expiry = Math.round(now / 1e6 + 1000);

  // Populate db with a first-party unpartitioned cookies
  let cookie = new Cookie(
    "test",
    "Some data",
    "example.com",
    "/",
    expiry,
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
    false // isPartitioned
  );
  database.insertCookie(cookie);

  // Populate db with a first-party unpartitioned cookies with the partitioned attribute
  cookie = new Cookie(
    "test partitioned",
    "Some data",
    "example.com",
    "/",
    expiry,
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
    true // isPartitioned
  );
  database.insertCookie(cookie);

  // Populate db with a first-party unpartitioned cookies with the partitioned attribute
  cookie = new Cookie(
    "test overwrite",
    "Overwritten",
    "example.com",
    "/",
    expiry,
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
    true // isPartitioned
  );
  database.insertCookie(cookie);

  // Populate db with a first-party unpartitioned cookies with the partitioned attribute
  cookie = new Cookie(
    "test overwrite",
    "Did not overwrite",
    "example.com",
    "/",
    expiry,
    now,
    now,
    false,
    false,
    false,
    false,
    { partitionKey: "(https,example.com)" },
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_UNSET,
    true // isPartitioned
  );
  database.insertCookie(cookie);

  database.close();
  database = null;

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.cookie.CHIPS.enabled");
    Services.prefs.clearUserPref("network.cookie.CHIPS.migrateDatabase");
  });

  // Reload profile.
  Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", true);
  Services.prefs.setIntPref("network.cookie.CHIPS.lastMigrateDatabase", 0);
  Services.prefs.setIntPref("network.cookie.CHIPS.migrateDatabaseTarget", 0);
  await promise_load_profile();

  // Make sure there were no changes
  Assert.equal(
    Services.cookies.getCookiesFromHost("example.com", {}).length,
    3
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {})
      .filter(cookie => cookie.name == "test").length,
    1
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {})
      .filter(cookie => cookie.name == "test partitioned").length,
    1
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {})
      .filter(cookie => cookie.name == "test overwrite").length,
    1
  );
  Assert.equal(
    Services.cookies.getCookiesFromHost("example.com", {
      partitionKey: "(https,example.com)",
    }).length,
    1
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {})
      .filter(cookie => cookie.name == "test overwrite").length,
    1
  );

  // Close the profile.
  await promise_close_profile();

  // Reload profile.
  await Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", true);
  await Services.prefs.setIntPref(
    "network.cookie.CHIPS.migrateDatabaseTarget",
    1000
  );
  await promise_load_profile();

  // Check if the first-party unpartitioned cookie is still there
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {})
      .filter(cookie => cookie.name == "test").length,
    1
  );

  // Check that we no longer have Partitioned cookies in the unpartitioned storage
  Assert.equal(
    Services.cookies.getCookiesFromHost("example.com", {}).length,
    1
  );

  // Check that we only have our two partitioned cookies
  Assert.equal(
    Services.cookies.getCookiesFromHost("example.com", {
      partitionKey: "(https,example.com)",
    }).length,
    2
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {
        partitionKey: "(https,example.com)",
      })
      .filter(cookie => cookie.name == "test").length,
    0
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {
        partitionKey: "(https,example.com)",
      })
      .filter(cookie => cookie.name == "test partitioned").length,
    1
  );
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {
        partitionKey: "(https,example.com)",
      })
      .filter(cookie => cookie.name == "test overwrite").length,
    1
  );

  // Test that we overwrote the value of the cookie in the partition with the
  // value that was not partitioned
  Assert.equal(
    Services.cookies
      .getCookiesFromHost("example.com", {
        partitionKey: "(https,example.com)",
      })
      .filter(cookie => cookie.name == "test overwrite")[0].value,
    "Overwritten"
  );

  // Make sure we cleared the migration pref as part of the migration
  Assert.equal(
    Services.prefs.getIntPref("network.cookie.CHIPS.lastMigrateDatabase"),
    1000
  );

  // Cleanup
  Services.cookies.removeAll();
  do_close_profile();
});
