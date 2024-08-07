/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("http://localhost");

  const resultBeforeInstall = {
    databaseUsage: 0,
    fileUsage: 0,
    usage: 0,
  };

  const resultAfterInstall = {
    databaseUsage: 147456,
    fileUsage: 0,
    usage: 147456,
  };

  function verifyResult(result, expectedResult) {
    ok(
      result instanceof Ci.nsIQuotaOriginUsageResult,
      "The result is nsIQuotaOriginUsageResult instance"
    );

    Assert.strictEqual(
      result.databaseUsage,
      expectedResult.databaseUsage,
      "Database usage equals"
    );
    Assert.strictEqual(
      result.fileUsage,
      expectedResult.fileUsage,
      "File usage equals"
    );
    Assert.strictEqual(
      result.usage,
      expectedResult.usage,
      "Total usage equals"
    );
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Getting origin usage");

  request = getOriginUsage(principal);
  let result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, resultBeforeInstall);

  info("Clearing");

  request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains IndexedDB databases placed across the repositories.
  // The file make_getUsageForPrincipal.js was run locally, specifically it was
  // temporarily enabled in xpcshell.ini and then executed:
  // mach test --interactive dom/quota/test/xpcshell/make_getUsageForPrincipal.js
  installPackage("getUsageForPrincipal_profile");

  info("Getting origin usage");

  request = getOriginUsage(principal);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, resultAfterInstall);

  info("Getting origin usage");

  Services.prefs.setIntPref(
    "dom.quotaManager.originOperations.pauseOnIOThreadMs",
    1000
  );

  request = getOriginUsage(principal);

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
