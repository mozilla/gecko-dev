/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

class CookieValidatedObserver {
  #promise;

  static waitForCookieValidation() {
    return new Promise(resolve => {
      new CookieValidatedObserver(resolve);
    });
  }

  constructor(promise) {
    this.#promise = promise;

    this.obs = Services.obs;
    this.obs.addObserver(this, "cookies-validated");
  }

  observe(subject, topic) {
    if (topic == "cookies-validated") {
      if (this.obs) {
        this.obs.removeObserver(this, "cookies-validated");
      }

      this.#promise();
    }
  }
}

add_task(async function test_invalid_cookie_fix() {
  let promise = CookieValidatedObserver.waitForCookieValidation();

  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies.sessionCookies;

  // Each cookie-db opening will trigger a validation.
  await promise;

  // Close the profile.
  await promise_close_profile();

  // Remove the cookie file in order to create another database file.
  do_get_cookie_file(profile).remove(false);

  // Create a schema 15 database.
  let schema15db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    15
  );

  let now = Date.now();

  schema15db.insertCookie(
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
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema15db.insertCookie(
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

  schema15db.insertCookie(
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

  schema15db.insertCookie(
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
      Ci.nsICookie.SAMESITE_UNSET,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema15db.insertCookie(
    new Cookie(
      "test5",
      "Some data",
      "foo.com",
      "/",
      now,
      now,
      now,
      false,
      true,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_UNSET
    )
  );

  schema15db.close();
  schema15db = null;

  // Check if we have the right entries
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite, isSecure, creationTime FROM moz_cookies"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({
        name: stmt.getString(0),
        sameSite: stmt.getInt32(1),
        isSecure: stmt.getInt32(2),
        creationTime: stmt.getInt64(3),
      });
    }

    Assert.deepEqual(results, [
      {
        name: "test1",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test2",
        sameSite: Ci.nsICookie.SAMESITE_LAX,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test3",
        sameSite: Ci.nsICookie.SAMESITE_STRICT,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test4",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test5",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: true,
        creationTime: now,
      },
    ]);

    stmt.finalize();
    dbConnection.close();
  }

  promise = CookieValidatedObserver.waitForCookieValidation();

  // Reload profile.
  await promise_load_profile();

  await promise;

  // Assert inserted cookies are in the db and correctly handled by services.
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), 5);

  // Check if the sameSite issues were fixed
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite, isSecure, creationTime FROM moz_cookies"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({
        name: stmt.getString(0),
        sameSite: stmt.getInt32(1),
        isSecure: stmt.getInt32(2),
        creationTime: stmt.getInt64(3),
      });
    }

    Assert.deepEqual(results, [
      {
        name: "test2",
        sameSite: Ci.nsICookie.SAMESITE_LAX,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test3",
        sameSite: Ci.nsICookie.SAMESITE_STRICT,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test4",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: false,
        creationTime: now,
      },
      {
        name: "test5",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: true,
        creationTime: now,
      },
      {
        name: "test1",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: false,
        creationTime: now,
      },
    ]);

    stmt.finalize();
    dbConnection.close();
  }

  // Cleanup
  Services.cookies.removeAll();
  do_close_profile();
});
