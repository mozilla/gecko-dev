/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
  Ci.nsIContentPrefService2
);

// Async wrappers around content pref service setter / getter. Would be nice if
// the service provided that itself.

async function setContentPref(domain, name, value, isPBM) {
  return new Promise((resolve, reject) => {
    let loadContext = isPBM
      ? Cu.createPrivateLoadContext()
      : Cu.createLoadContext();
    cps2.set(domain, name, value, loadContext, {
      handleCompletion: aReason => {
        if (aReason === cps2.COMPLETE_ERROR) {
          reject(new Error("Failed to set content pref"));
        } else {
          resolve();
        }
      },
    });
  });
}

async function getContentPref(domain, name, isPBM) {
  return new Promise((resolve, reject) => {
    let loadContext = isPBM
      ? Cu.createPrivateLoadContext()
      : Cu.createLoadContext();
    let result;
    cps2.getByDomainAndName(domain, name, loadContext, {
      handleResult({ value }) {
        result = value;
      },
      handleCompletion(aReason) {
        if (aReason === cps2.COMPLETE_ERROR) {
          reject(new Error("Failed to get content pref"));
        } else {
          resolve(result);
        }
      },
    });
  });
}

add_task(async function test_deleteByHost() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "bar", "bar", true);
  await setContentPref("foo.example.org", "bar", "bar", false);
  await setContentPref("foo.example.org", "bar", "bar", true);
  await setContentPref("bar.foo.example.org", "subsub", "subsub", false);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "bar", true), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", true), "bar");
  Assert.equal(
    await getContentPref("bar.foo.example.org", "subsub", false),
    "subsub"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "foo.example.org",
      true,
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info(
    "Verify content prefs matching host 'foo.example.org' have been cleared"
  );
  Assert.equal(
    await getContentPref("example.com", "foo", false),
    "foo",
    "Unrelated domain entry should still exist."
  );
  Assert.equal(
    await getContentPref("example.org", "bar", false),
    "bar",
    "Base domain entry should still exist."
  );
  Assert.equal(
    await getContentPref("example.org", "bar", true),
    "bar",
    "Base domain PBM entry should still exist."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    undefined,
    "Exact domain match should have been cleared in normal browsing."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "bar", true),
    "bar",
    "Exact domain match should not have been cleared in private browsing."
  );
  Assert.equal(
    await getContentPref("bar.foo.example.org", "subsub", false),
    undefined,
    "Subdomain should have been cleared"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteByPrincipal() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "bar", "bar", true);
  await setContentPref("foo.example.org", "bar", "bar", false);
  await setContentPref("foo.example.org", "bar", "bar", true);
  await setContentPref("bar.foo.example.org", "subsub", "subsub", false);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "bar", true), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", true), "bar");
  Assert.equal(
    await getContentPref("bar.foo.example.org", "subsub", false),
    "subsub"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "https://foo.example.org"
      ),
      true,
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info(
    "Verify content prefs matching principal 'https://foo.example.org' have been cleared"
  );
  Assert.equal(
    await getContentPref("example.com", "foo", false),
    "foo",
    "Unrelated domain entry should still exist."
  );
  Assert.equal(
    await getContentPref("example.org", "bar", false),
    "bar",
    "Base domain entry should still exist."
  );
  Assert.equal(
    await getContentPref("example.org", "bar", true),
    "bar",
    "Base domain PBM entry should still exist."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    undefined,
    "Exact domain match should have been cleared in normal browsing."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "bar", true),
    "bar",
    "Exact domain match should NOT have been cleared in private browsing."
  );
  // TODO: PreferencesCleaner does not clear by exact principal but includes
  // subdomains of the given principal.
  Assert.equal(
    await getContentPref("bar.foo.example.org", "subsub", false),
    undefined,
    "TODO: Subdomain entry should still exist."
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteBySite() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "bar", "bar", true);
  await setContentPref("foo.example.org", "bar", "bar", false);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "bar", true), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.org",
      {},
      true,
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info(
    "Verify content prefs for 'example.org' have been cleared, including PBM."
  );
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), undefined);
  Assert.equal(await getContentPref("example.org", "bar", true), undefined);
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    undefined
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteBySite_pattern() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "barPBM", "barPBM", true);
  await setContentPref("foo.example.org", "bar", "bar", false);
  await setContentPref("foo.example.org", "subPBM", "subPBM", true);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "barPBM", true), "barPBM");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");
  Assert.equal(
    await getContentPref("foo.example.org", "subPBM", true),
    "subPBM"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.org",
      { privateBrowsingId: 1 },
      true,
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info(
    "Verify content prefs for 'example.org' have been cleared, but only for PBM."
  );
  Assert.equal(
    await getContentPref("example.com", "foo", false),
    "foo",
    "Unrelated domain should have not been cleared."
  );
  Assert.equal(
    await getContentPref("example.org", "bar", false),
    "bar",
    "Base domain entry should NOT have been cleared for normal browsing."
  );
  Assert.equal(
    await getContentPref("example.org", "barPBM", true),
    undefined,
    "Base domain entry should have been cleared for private browsing."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    "bar",
    "Subdomain entry should not have been cleared for normal browsing."
  );
  Assert.equal(
    await getContentPref("foo.example.org", "subPBM", true),
    undefined,
    "Subdomain entry should have been cleared for private browsing."
  );

  await SiteDataTestUtils.clear();
});

// TODO: implement a proper range clearing test. We're currently lacking the
// capability to set content prefs with a specific creation timestamp. This
// tests only clearing everything if the entire time range is passed.
add_task(async function test_deleteByRange() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "bar", "bar", true);
  await setContentPref("foo.example.org", "bar", "bar", false);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "bar", true), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");

  info("Delete entire time range.");
  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      0,
      Date.now() * 1000,
      true,
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info("Verify all content prefs have been cleared");
  Assert.equal(await getContentPref("example.com", "foo", false), undefined);
  Assert.equal(await getContentPref("example.org", "bar", false), undefined);
  Assert.equal(await getContentPref("example.org", "bar", true), undefined);
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    undefined
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteAll() {
  info("Set content prefs");
  await setContentPref("example.com", "foo", "foo", false);
  await setContentPref("example.org", "bar", "bar", false);
  await setContentPref("example.org", "bar", "bar", true);
  await setContentPref("foo.example.org", "bar", "bar", false);

  info("Verify content prefs have been set");
  Assert.equal(await getContentPref("example.com", "foo", false), "foo");
  Assert.equal(await getContentPref("example.org", "bar", false), "bar");
  Assert.equal(await getContentPref("example.org", "bar", true), "bar");
  Assert.equal(await getContentPref("foo.example.org", "bar", false), "bar");

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  info("Verify all content prefs have been cleared");
  Assert.equal(await getContentPref("example.com", "foo", false), undefined);
  Assert.equal(await getContentPref("example.org", "bar", false), undefined);
  Assert.equal(await getContentPref("example.org", "bar", true), undefined);
  Assert.equal(
    await getContentPref("foo.example.org", "bar", false),
    undefined
  );

  await SiteDataTestUtils.clear();
});
