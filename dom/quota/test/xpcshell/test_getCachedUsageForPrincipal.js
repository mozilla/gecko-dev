/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("http://localhost");

  const resultBeforeInstall = 0;
  const resultAfterInstall = 0;
  const resultAfterInitializeStorage = 0;
  const resultAfterInitializeTemporaryStorage = 98304;

  function verifyResult(result, expectedResult) {
    Assert.strictEqual(result, expectedResult, "Cached usage equals");
  }

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Getting cached origin usage");

  request = getCachedOriginUsage(principal);
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
  // mach test --interactive dom/quota/test/xpcshell/make_getCachedUsageForPrincipal.js
  installPackage("getCachedUsageForPrincipal_profile");

  info("Getting cached origin usage");

  request = getCachedOriginUsage(principal);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, resultAfterInstall);

  info("Initializing storage");

  request = init();
  await requestFinished(request);

  info("Getting cached origin usage");

  request = getCachedOriginUsage(principal);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, resultAfterInitializeStorage);

  info("Initializing temporary storage");

  request = initTemporaryStorage();
  await requestFinished(request);

  info("Getting cached origin usage");

  request = getCachedOriginUsage(principal);
  result = await requestFinished(request);

  info("Verifying result");

  verifyResult(result, resultAfterInitializeTemporaryStorage);
}
