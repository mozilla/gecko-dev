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

  do_send_remote_message("LocalStorageTest::ChildReady");

  try {
    storage.open();
  } catch (ex) {}
});
