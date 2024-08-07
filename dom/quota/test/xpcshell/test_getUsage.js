/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const origins = [
    {
      origin: "http://example.com",
      persisted: false,
      usage: 49152,
    },

    {
      origin: "http://localhost",
      persisted: false,
      usage: 147456,
    },

    {
      origin: "http://www.mozilla.org",
      persisted: true,
      usage: 98304,
    },
  ];

  const allOrigins = [
    {
      origin: "chrome",
      persisted: false,
      usage: 147456,
    },

    {
      origin: "http://example.com",
      persisted: false,
      usage: 49152,
    },

    {
      origin: "http://localhost",
      persisted: false,
      usage: 147456,
    },

    {
      origin: "http://www.mozilla.org",
      persisted: true,
      usage: 98304,
    },
  ];

  function verifyResult(result, expectedOrigins) {
    ok(result instanceof Array, "Got an array object");
    Assert.equal(
      result.length,
      expectedOrigins.length,
      "Correct number of elements"
    );

    info("Sorting elements");

    result.sort(function (a, b) {
      let originA = a.origin;
      let originB = b.origin;

      if (originA < originB) {
        return -1;
      }
      if (originA > originB) {
        return 1;
      }
      return 0;
    });

    info("Verifying elements");

    for (let i = 0; i < result.length; i++) {
      let a = result[i];
      let b = expectedOrigins[i];
      Assert.equal(a.origin, b.origin, "Origin equals");
      Assert.equal(a.persisted, b.persisted, "Persisted equals");
      Assert.equal(a.usage, b.usage, "Usage equals");
    }
  }

  function dummy() {}

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Getting usage");

  request = getUsage(dummy, /* getAll */ true);
  let result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, []);

  info("Clearing");

  request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains IndexedDB databases placed across the repositories.
  // The file create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  // mach xpcshell-test --interactive dom/quota/test/xpcshell/create_db.js
  installPackage("getUsage_profile");

  info("Getting usage");

  request = getUsage(dummy, /* getAll */ false);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, origins);

  info("Getting usage");

  request = getUsage(dummy, /* getAll */ true);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, allOrigins);

  info("Getting usage");

  Services.prefs.setIntPref(
    "dom.quotaManager.originOperations.pauseOnIOThreadMs",
    1000
  );

  request = getUsage(dummy, /* getAll */ true);

  info("Cancelling request");

  request.cancel();

  try {
    result = await requestFinished(request);
    ok(false, "Should have thrown");
  } catch (e) {
    ok(true, "Should have thrown");
    Assert.strictEqual(
      e.resultCode,
      NS_ERROR_FAILURE,
      "Threw right result code"
    );
  }

  Services.prefs.clearUserPref(
    "dom.quotaManager.originOperations.pauseOnIOThreadMs"
  );
}
