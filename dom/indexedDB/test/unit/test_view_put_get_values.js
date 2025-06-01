/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps, disableWorkerTest */
var disableWorkerTest = "Need a way to set temporary prefs from a worker";

async function testSteps() {
  // Setting dom.indexedDB.dataThreshold to 99999 ensures that the first random
  // view (size 100000) is stored as a separate file when
  // dom.indexedDB.preprocessing is true. The second random view (size 10000)
  // is always stored directly in the database. This setup creates a scenario
  // where requests in a transaction are processed in the wrong order if they
  // are not properly queued internally.
  const dataThreshold = 99999;

  const name = this.window
    ? window.location.pathname
    : "test_view_put_get_values.js";

  const objectStoreName = "Views";

  const viewDataArray = [
    { key: 1, view: getRandomView(100000) },
    { key: 2, view: getRandomView(10000) },
  ];

  const tests = [
    {
      external: false,
      preprocessing: false,
    },
    {
      external: true,
      preprocessing: false,
    },
    {
      external: true,
      preprocessing: true,
    },
  ];

  for (let test of tests) {
    if (test.external) {
      info("Setting data threshold pref");

      if (this.window) {
        await SpecialPowers.pushPrefEnv({
          set: [["dom.indexedDB.dataThreshold", dataThreshold]],
        });
      } else {
        setDataThreshold(dataThreshold);
      }
    }

    if (test.preprocessing) {
      info("Setting preprocessing pref");

      if (this.window) {
        await SpecialPowers.pushPrefEnv({
          set: [["dom.indexedDB.preprocessing", true]],
        });
      } else {
        enablePreprocessing();
      }
    }

    info("Opening database");

    const db = await (async function () {
      const request = indexedDB.open(name);

      {
        const event = await expectingUpgrade(request);

        const database = event.target.result;

        database.createObjectStore(objectStoreName);
      }

      const event = await expectingSuccess(request);

      const database = event.target.result;

      return database;
    })();

    {
      const objectStore = db
        .transaction([objectStoreName], "readwrite")
        .objectStore(objectStoreName);

      info("Storing views");

      for (const viewData of viewDataArray) {
        const request = objectStore.add(viewData.view, viewData.key);

        await requestSucceeded(request);

        is(request.result, viewData.key, "Got correct key");
      }

      info("Getting views");

      for (const viewData of viewDataArray) {
        const request = objectStore.get(viewData.key);

        await requestSucceeded(request);

        verifyView(request.result, viewData.view);
      }
    }

    info("Getting views in separate transactions");

    for (const viewData of viewDataArray) {
      const request = db
        .transaction([objectStoreName])
        .objectStore(objectStoreName)
        .get(viewData.key);

      await requestSucceeded(request);

      verifyView(request.result, viewData.view);
    }

    info("Getting file usage");

    {
      const fileUsage = await new Promise(function (resolve) {
        getCurrentUsage(function (request) {
          resolve(request.result.fileUsage);
        });
      });

      if (test.external) {
        ok(fileUsage > 0, "File usage is not zero");
      } else {
        ok(fileUsage == 0, "File usage is zero");
      }
    }

    info("Getting views in parallel");

    {
      const objectStore = db
        .transaction([objectStoreName])
        .objectStore(objectStoreName);

      const promises = [];
      const keys = [];

      for (const viewData of viewDataArray) {
        const request = objectStore.get(viewData.key);

        promises.push(
          requestSucceeded(request, function () {
            keys.push(viewData.key);
          })
        );
      }

      await Promise.all(promises);

      is(keys.length, viewDataArray.length, "Correct number of keys");

      for (let i = 0; i < keys.length; i++) {
        is(keys[i], viewDataArray[i].key, "Correct key");
      }
    }

    info("Deleting database");

    {
      db.close();
      const request = indexedDB.deleteDatabase(name);
      await expectingSuccess(request);
    }

    info("Resetting prefs");

    if (this.window) {
      await SpecialPowers.popPrefEnv();
    } else {
      if (test.external) {
        resetDataThreshold();
      }

      if (test.preprocessing) {
        resetPreprocessing();
      }
    }
  }
}
