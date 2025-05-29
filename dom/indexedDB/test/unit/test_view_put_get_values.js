/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps, disableWorkerTest */
var disableWorkerTest = "Need a way to set temporary prefs from a worker";

async function testSteps() {
  const name = this.window
    ? window.location.pathname
    : "test_view_put_get_values.js";

  const objectStoreName = "Views";

  const viewData = { key: 1, view: getRandomView(100000) };

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
          set: [["dom.indexedDB.dataThreshold", 0]],
        });
      } else {
        setDataThreshold(0);
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

      info("Storing view");

      {
        const request = objectStore.add(viewData.view, viewData.key);

        await requestSucceeded(request);

        is(request.result, viewData.key, "Got correct key");
      }

      info("Getting view");

      {
        const request = objectStore.get(viewData.key);

        await requestSucceeded(request);

        verifyView(request.result, viewData.view);
      }
    }

    info("Getting view in new transaction");

    {
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
