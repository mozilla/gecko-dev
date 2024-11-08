/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { QuotaUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/QuotaUtils.sys.mjs"
);

async function testSteps() {
  const origins = [
    "https://example.com",
    "https://localhost",
    "https://www.mozilla.org",
  ];

  function verifyResult(result, expectedOrigins) {
    ok(Array.isArray(result), "Got an array object");
    Assert.equal(
      result.length,
      expectedOrigins.length,
      "Correct number of elements"
    );

    info("Sorting elements");

    result.sort(function (a, b) {
      if (a < b) {
        return -1;
      }
      if (a > b) {
        return 1;
      }
      return 0;
    });

    info("Verifying elements");

    for (let i = 0; i < result.length; i++) {
      Assert.equal(
        result[i],
        expectedOrigins[i],
        "Result matches expected origin"
      );
    }
  }

  info("Clearing");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }

  info("Listing cached origins");

  const originsBeforeInit = await (async function () {
    const request = Services.qms.listCachedOrigins();
    const result = await QuotaUtils.requestFinished(request);
    return result;
  })();

  info("Verifying result");

  verifyResult(originsBeforeInit, []);

  info("Clearing");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing");

  {
    const request = Services.qms.init();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing temporary storage");

  {
    const request = Services.qms.initTemporaryStorage();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing origins");

  for (const origin of origins) {
    const request = Services.qms.initializeTemporaryOrigin(
      "default",
      PrincipalUtils.createPrincipal(origin),
      /* aCreateIfNonExistent */ true
    );
    await QuotaUtils.requestFinished(request);
  }

  info("Listing cached origins");

  const originsAfterInit = await (async function () {
    const request = Services.qms.listCachedOrigins();
    const result = await QuotaUtils.requestFinished(request);
    return result;
  })();

  info("Verifying result");

  verifyResult(originsAfterInit, origins);
}
