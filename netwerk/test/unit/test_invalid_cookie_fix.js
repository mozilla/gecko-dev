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

  // Create a schema 16 database.
  let schema16db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    16
  );

  let now = Date.now();
  let future = now + 60 * 60 * 24 * 1000 * 1000;

  // CookieValidation.result => eRejectedNoneRequiresSecure
  schema16db.insertCookie(
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

  // CookieValidation.result => eOK
  schema16db.insertCookie(
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

  // CookieValidation.result => eOK
  schema16db.insertCookie(
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

  // CookieValidation.result => eOK
  schema16db.insertCookie(
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

  // CookieValidation.result => eRejectedNoneRequiresSecure
  schema16db.insertCookie(
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

  // CookieValidation.result => eRejectedAttributeExpiryOversize
  schema16db.insertCookie(
    new Cookie(
      "test6",
      "Some data",
      "foo.com",
      "/",
      future,
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

  // CookieValidation.result => eRejectedEmptyNameAndValue
  schema16db.insertCookie(
    new Cookie(
      "",
      "",
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

  // CookieValidation.result => eRejectedInvalidCharName
  schema16db.insertCookie(
    new Cookie(
      " test8",
      "",
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

  // CookieValidation.result => eRejectedInvalidCharValue
  schema16db.insertCookie(
    new Cookie(
      "test9",
      " test9",
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

  schema16db.close();
  schema16db = null;

  // Check if we have the right entries
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite, isSecure, creationTime, expiry FROM moz_cookies ORDER BY name"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({
        name: stmt.getString(0),
        sameSite: stmt.getInt32(1),
        isSecure: stmt.getInt32(2),
        creationTime: stmt.getInt64(3),
        expiry: stmt.getInt64(4),
      });
    }

    Assert.deepEqual(results, [
      {
        name: "",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: " test8",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test1",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test2",
        sameSite: Ci.nsICookie.SAMESITE_LAX,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test3",
        sameSite: Ci.nsICookie.SAMESITE_STRICT,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test4",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test5",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: 1,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test6",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: future,
      },
      {
        name: "test9",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
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
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), 6);

  // Close the profile.
  await promise_close_profile();

  // Check if the sameSite issues were fixed
  {
    const dbConnection = Services.storage.openDatabase(
      do_get_cookie_file(profile)
    );
    const stmt = dbConnection.createStatement(
      "SELECT name, sameSite, isSecure, creationTime, expiry FROM moz_cookies ORDER BY name"
    );

    const results = [];
    while (stmt.executeStep()) {
      results.push({
        name: stmt.getString(0),
        sameSite: stmt.getInt32(1),
        isSecure: stmt.getInt32(2),
        creationTime: stmt.getInt64(3),
        expiry: stmt.getInt64(4),
      });
    }

    Assert.deepEqual(results, [
      {
        name: "test1",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test2",
        sameSite: Ci.nsICookie.SAMESITE_LAX,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test3",
        sameSite: Ci.nsICookie.SAMESITE_STRICT,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test4",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test5",
        sameSite: Ci.nsICookie.SAMESITE_NONE,
        isSecure: 1,
        creationTime: now,
        expiry: now,
      },
      {
        name: "test6",
        sameSite: Ci.nsICookie.SAMESITE_UNSET,
        isSecure: 0,
        creationTime: now,
        expiry: results.find(a => a.name === "test6").expiry,
      },
    ]);

    for (const r of results) {
      Assert.less(r.expiry, future);
    }

    stmt.finalize();
    dbConnection.close();
  }

  // Cleanup
  await promise_load_profile();
  Services.cookies.removeAll();
  do_close_profile();
});
