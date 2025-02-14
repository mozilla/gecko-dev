"use strict";

const URL = "http://example.net";
const URL_SUBDOMAIN = "http://subdomain.example.net";
const URL2 = "http://foo.bar";

/**
 * Helper to wrap the OpenCallback in a Promise.
 */
function OpenCallbackPromise(behavior, workingMetadata, workingData, url) {
  return new Promise(resolve => {
    asyncOpenCacheEntry(
      url,
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      null,
      new OpenCallback(behavior, workingMetadata, workingData, resolve)
    );
  });
}

async function run_test() {
  do_get_profile();
  do_test_pending();

  try {
    info(`Create first entry for ${URL}/a`);
    await OpenCallbackPromise(NEW, "e1m", "e1d", URL + "/a");

    info(`Verify first entry for ${URL}/a`);
    await OpenCallbackPromise(NORMAL, "e1m", "e1d", URL + "/a");

    info(`Create entry for ${URL_SUBDOMAIN}/a`);
    await OpenCallbackPromise(NEW, "es1m", "es1d", URL_SUBDOMAIN + "/a");

    info(`Verify entry for ${URL_SUBDOMAIN}/a`);
    await OpenCallbackPromise(NORMAL, "es1m", "es1d", URL_SUBDOMAIN + "/a");

    info(`Create entry for ${URL2}/a`);
    await OpenCallbackPromise(NEW, "f1m", "f1d", URL2 + "/a");

    info(`Verify entry for ${URL2}/a`);
    await OpenCallbackPromise(NORMAL, "f1m", "f1d", URL2 + "/a");

    info(`Clear base domain associated with ${URL}`);
    const url = Services.io.newURI(URL);
    const principal = Services.scriptSecurityManager.createContentPrincipal(
      url,
      {}
    );
    Services.cache2.clearBaseDomain(principal.baseDomain);

    info(`${URL}/a entry should be new after clearing`);
    await OpenCallbackPromise(NEW, "e1m", "e1d", URL + "/a");

    info(`${URL_SUBDOMAIN}/a entry should be new after clearing`);
    await OpenCallbackPromise(NEW, "es1m", "es1d", URL_SUBDOMAIN + "/a");

    info(`${URL2}/a entry should still exist`);
    await OpenCallbackPromise(NORMAL, "f1m", "f1d", URL2 + "/a");

    finish_cache2_test();
  } catch (e) {
    do_throw(e);
  }
}
