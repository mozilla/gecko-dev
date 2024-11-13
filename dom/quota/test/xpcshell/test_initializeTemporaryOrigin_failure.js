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
const { SimpleDBUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/simpledb/test/modules/SimpleDBUtils.sys.mjs"
);

async function testCachedOrigins() {
  const origin = "https://example.com";
  const principal = PrincipalUtils.createPrincipal(origin);

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

  info("Initializing origin");

  const ex = await QuotaUtils.withArtificialFailures(
    Ci.nsIQuotaArtificialFailure.CATEGORY_CREATE_DIRECTORY_METADATA2,
    /* probability */ 100,
    Cr.NS_ERROR_UNEXPECTED,
    async function () {
      const request = Services.qms.initializeTemporaryOrigin(
        "default",
        principal,
        /* aCreateIfNonExistent */ true
      );
      try {
        await QuotaUtils.requestFinished(request);
        return null;
      } catch (ex) {
        return ex;
      }
    }
  );

  Assert.ok(ex, "Should have thrown");
  Assert.strictEqual(
    ex.resultCode,
    NS_ERROR_UNEXPECTED,
    "Threw right result code"
  );

  info("Listing origins");

  const origins = await (async function () {
    const request = Services.qms.listOrigins();
    const result = await QuotaUtils.requestFinished(request);
    return result;
  })();

  Assert.equal(origins.length, 1, "Returned one origin");
  Assert.equal(origins[0], origin, "Returned correct origin");

  info("Listing cached origins");

  const cachedOrigins = await (async function () {
    const request = Services.qms.listCachedOrigins();
    const result = await QuotaUtils.requestFinished(request);
    return result;
  })();

  Assert.equal(cachedOrigins.length, 1, "Returned one origin");
  Assert.equal(cachedOrigins[0], origin, "Returned correct origin");

  info("Clearing storage");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }
}

async function testCreateConnection() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_initializeTemporaryOrigin_failure.js";

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

  info("Initializing group");

  {
    const request = Services.qms.initializeTemporaryGroup(principal);
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing origin");

  const ex = await QuotaUtils.withArtificialFailures(
    Ci.nsIQuotaArtificialFailure.CATEGORY_CREATE_DIRECTORY_METADATA2,
    /* probability */ 100,
    Cr.NS_ERROR_UNEXPECTED,
    async function () {
      const request = Services.qms.initializeTemporaryOrigin(
        "default",
        principal,
        /* aCreateIfNonExistent */ true
      );
      try {
        await QuotaUtils.requestFinished(request);
        return null;
      } catch (ex) {
        return ex;
      }
    }
  );

  Assert.ok(ex, "Should have thrown");
  Assert.strictEqual(
    ex.resultCode,
    NS_ERROR_UNEXPECTED,
    "Threw right result code"
  );

  info("Opening database");

  {
    const connection = SimpleDBUtils.createConnection(principal);
    const request = connection.open(name);
    await SimpleDBUtils.requestFinished(request);
  }

  info("Clearing storage");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }
}

/* exported testSteps */
async function testSteps() {
  add_task(testCachedOrigins);
  add_task(testCreateConnection);
}
