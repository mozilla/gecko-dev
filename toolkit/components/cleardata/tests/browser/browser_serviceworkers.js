/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

async function addServiceWorker(
  origin,
  topLevelOrigin = null,
  userContextId = null
) {
  let swURL =
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    "worker.js";

  let registered = SiteDataTestUtils.promiseServiceWorkerRegistered(swURL);
  await SiteDataTestUtils.addServiceWorker({
    win: window,
    path: swURL,
    topLevelPath: topLevelOrigin,
    userContextId,
  });
  await registered;

  ok(true, `${origin} has a service worker`);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });
});

add_task(async function test_deleteFromHost() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://example.org");
  await addServiceWorker("https://test1.example.org");

  let unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://example.com"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.com",
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "example.com has no service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has a service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://test1.example.org"),
    "test1.example.org has a service worker"
  );

  // Clearing the subdomain should not clear the entire site.
  unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test1.example.org"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "test1.example.org",
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.org"),
    "test1.example.org has no service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has a service worker"
  );

  unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://example.org"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.org",
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "example.com has no service worker"
  );
});

add_task(async function test_deleteFromSite() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://test1.example.com");
  await addServiceWorker("https://test2.example.com");

  // Both of these partitioned service workers should be removed by
  // deleteDataFromSite targeting 'example.com'. example.org sw partitioned
  // under example.com
  await addServiceWorker(
    "https://sub1.test1.example.org",
    "https://example.com"
  );
  // example.com sw partitioned under example.org
  await addServiceWorker("https://www.example.com", "https://example.org");

  await addServiceWorker("https://example.org");

  let unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://example.com"
  );
  let unregisteredSub1 = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test1.example.com"
  );
  let unregisteredSub2 = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test2.example.com"
  );
  let unregisteredPartitioned1 =
    SiteDataTestUtils.promiseServiceWorkerUnregistered(
      "https://sub1.test1.example.org"
    );
  let unregisteredPartitioned2 =
    SiteDataTestUtils.promiseServiceWorkerUnregistered(
      "https://www.example.com"
    );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.com",
      {}, // All OA
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(
          value,
          0,
          "Call to nsIClearDataService should not return any errors."
        );
        aResolve();
      }
    );
  });
  await Promise.all([
    unregistered,
    unregisteredSub1,
    unregisteredSub2,
    unregisteredPartitioned1,
    unregisteredPartitioned2,
  ]);

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.com"),
    "test1.example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test2.example.com"),
    "test2.example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://sub1.test1.example.org"),
    "sub1.test1.example.org has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://www.example.com"),
    "www.example.com has no service worker"
  );

  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has a service worker"
  );

  unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://example.org"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.org",
      {},
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.com"),
    "test1.example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test2.example.com"),
    "test2.example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://sub1.test1.example.org"),
    "sub1.test1.example.org has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://www.example.com"),
    "www.example.com has no service worker"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteFromSiteWithPattern() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://test1.example.com");
  await addServiceWorker("https://test2.example.com");

  // Both of these partitioned service workers should be removed by
  // deleteDataFromSite targeting 'example.com'. example.org sw partitioned
  // under example.com
  await addServiceWorker(
    "https://sub1.test1.example.org",
    "https://example.com"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://sub1.test1.example.org"),
    "sub1.test1.example.org initially has a service worker"
  );
  // example.com sw partitioned under example.org
  await addServiceWorker("https://www.example.com", "https://example.org", 3);

  await addServiceWorker("https://example.org");

  let unregisteredSub2 = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://www.example.com"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.com",
      { userContextId: 3 }, // Only OA with userContextId == 3
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await Promise.all([unregisteredSub2]);

  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "example.com has a service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://test1.example.com"),
    "test1.example.com has a service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://test2.example.com"),
    "test2.example.com has a service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://sub1.test1.example.org"),
    "sub1.test1.example.org has a service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://www.example.com"),
    "www.example.com has no service worker"
  );

  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "example.org has a service worker"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteFromPrincipal() {
  await addServiceWorker("https://test1.example.com");
  await addServiceWorker("https://test1.example.org");

  let unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test1.example.com"
  );
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://test1.example.com/"
    );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.com"),
    "test1.example.com has no service worker"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://test1.example.org"),
    "test1.example.org has a service worker"
  );

  unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test1.example.org"
  );
  principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://test1.example.org/"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.org"),
    "test1.example.org has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test1.example.com"),
    "test1.example.com has no service worker"
  );
});

add_task(async function test_deleteAll() {
  await addServiceWorker("https://test2.example.com");
  await addServiceWorker("https://test2.example.org");

  let unregistered = SiteDataTestUtils.promiseServiceWorkerUnregistered(
    "https://test2.example.com"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });
  await unregistered;

  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test2.example.com"),
    "test2.example.com has no service worker"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://test2.example.org"),
    "test2.example.org has no service worker"
  );

  await SiteDataTestUtils.clear();
});
