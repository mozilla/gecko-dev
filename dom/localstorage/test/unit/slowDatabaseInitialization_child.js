/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);

const { LocalStorageUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/localstorage/test/modules/LocalStorageUtils.sys.mjs"
);

add_task(async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  const storage = LocalStorageUtils.createStorage(principal);

  try {
    storage.getItem("foo");
    ok(false, "Should have thrown");
  } catch (e) {
    ok(true, "Should have thrown");
    Assert.strictEqual(e.result, Cr.NS_ERROR_ABORT, "Threw right result code");
  }
});
