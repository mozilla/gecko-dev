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
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/* exported testSteps */
async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  info("Initializing storage");

  {
    const request = Services.qms.init();
    await QuotaUtils.requestFinished(request);
  }

  info("Starting persistent origin initialization");

  const initPromise = (async function () {
    const request = Services.qms.initializePersistentOrigin(principal);
    const promise = QuotaUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for origin initialization to start");

  await TestUtils.topicObserved("QuotaManager::OriginInitializationStarted");

  info("Waiting for origin initialization to finish");

  await initPromise;
}
