/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_setup(async function () {
  const { setStoragePrefs, clearStoragePrefs } = ChromeUtils.importESModule(
    "resource://testing-common/dom/quota/test/modules/StorageUtils.sys.mjs"
  );

  setStoragePrefs();

  const optionalPrefsToSet = [["dom.simpleDB.enabled", true]];

  setStoragePrefs(optionalPrefsToSet);

  registerCleanupFunction(async function () {
    const optionalPrefsToClear = ["dom.simpleDB.enabled"];

    clearStoragePrefs(optionalPrefsToClear);
  });

  do_get_profile();
});
