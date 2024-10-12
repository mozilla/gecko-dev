/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { IndexedDBUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/indexedDB/test/modules/IndexedDBUtils.sys.mjs"
);
const { LocalStorageUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/localstorage/test/modules/LocalStorageUtils.sys.mjs"
);
const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { QuotaUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/QuotaUtils.sys.mjs"
);

async function testNonExistentOriginDirectory() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_quotaClientInteractions.js";
  const objectStoreName = "foo";

  info("Opening LocalStorage database");

  {
    const storage = LocalStorageUtils.createStorage(principal);
    storage.open();
  }

  info("Opening IndexedDB database");

  {
    const request = indexedDB.openForPrincipal(principal, name);
    request.onupgradeneeded = function (event) {
      const database = event.target.result;
      database.createObjectStore(objectStoreName);
    };
    const database = await IndexedDBUtils.requestFinished(request);
    database.close();
  }

  info("Resetting storage");

  {
    const request = Services.qms.reset();
    await QuotaUtils.requestFinished(request);
  }

  info("Opening LocalStorage database");

  {
    const storage = LocalStorageUtils.createStorage(principal);
    storage.open();
  }

  info("Deleting IndexedDB database");

  {
    const request = indexedDB.deleteForPrincipal(principal, name);
    await IndexedDBUtils.requestFinished(request);
  }
}

/* exported testSteps */
async function testSteps() {
  add_task(
    {
      pref_set: [
        ["dom.storage.testing", true],
        ["dom.storage.client_validation", false],
      ],
    },
    testNonExistentOriginDirectory
  );
}
