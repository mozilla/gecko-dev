/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(3);

add_task(async function testAllow() {
  await runTest(async (params, observeAllPromise, apply) => {
    assertListContents(params, []);

    params.url.value = "test.com";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["http://test.com", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlock() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "test.com";
    params.btnBlock.doCommand();

    assertListContents(params, [
      ["http://test.com", params.denyL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgain() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "test.com";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["http://test.com", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemove() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.richlistbox.selectedIndex = 0;
    params.btnRemove.doCommand();

    assertListContents(params, []);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "deleted" }]);
});

add_task(async function testAdd() {
  await runTest(async (params, observeAllPromise, apply) => {
    let uri = Services.io.newURI("http://test.com");
    Services.perms.add(uri, "popup", Ci.nsIPermissionManager.DENY_ACTION);

    info("Adding unrelated permission should not change display.");
    assertListContents(params, []);

    apply();
    await observeAllPromise;

    Services.perms.remove(uri, "popup");
  }, [{ type: "popup", origin: "http://test.com", data: "added",
        capability: Ci.nsIPermissionManager.DENY_ACTION }]);
});

add_task(async function testAllowHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["https://test.com:12345", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlockHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnBlock.doCommand();

    assertListContents(params, [
      ["https://test.com:12345", params.denyL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgainHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["https://test.com:12345", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemoveHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.richlistbox.selectedIndex = 0;
    params.btnRemove.doCommand();

    assertListContents(params, []);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "deleted" }]);
});

add_task(async function testAllowPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["http://localhost:12345", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlockPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnBlock.doCommand();

    assertListContents(params, [
      ["http://localhost:12345", params.denyL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgainPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnAllow.doCommand();

    assertListContents(params, [
      ["http://localhost:12345", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemovePort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.richlistbox.selectedIndex = 0;
    params.btnRemove.doCommand();

    assertListContents(params, []);

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "deleted" }]);
});

add_task(async function testSort() {
  await runTest(async (params, observeAllPromise, apply) => {
    // Sort by site name.
    EventUtils.synthesizeMouseAtCenter(params.doc.getElementById("siteCol"), {},
                                       params.doc.defaultView);

    for (let URL of ["http://a", "http://z", "http://b"]) {
      let URI = Services.io.newURI(URL);
      Services.perms.add(URI, "cookie", Ci.nsIPermissionManager.ALLOW_ACTION);
    }

    assertListContents(params, [
      ["http://a", params.allowL10nId],
      ["http://b", params.allowL10nId],
      ["http://z", params.allowL10nId],
    ]);

    // Sort by site name in descending order.
    EventUtils.synthesizeMouseAtCenter(params.doc.getElementById("siteCol"), {},
                                       params.doc.defaultView);

    assertListContents(params, [
      ["http://z", params.allowL10nId],
      ["http://b", params.allowL10nId],
      ["http://a", params.allowL10nId],
    ]);

    apply();
    await observeAllPromise;

    for (let URL of ["http://a", "http://z", "http://b"]) {
      let uri = Services.io.newURI(URL);
      Services.perms.remove(uri, "cookie");
    }
  }, [{ type: "cookie", origin: "http://a", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
      { type: "cookie", origin: "http://z", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
      { type: "cookie", origin: "http://b", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

function assertListContents(params, expected) {
  Assert.equal(params.richlistbox.itemCount, expected.length);
  for (let i = 0; i < expected.length; i++) {
    let richlistitem = params.richlistbox.getItemAtIndex(i);
    Assert.equal(richlistitem.getAttribute("origin"), expected[i][0]);
    Assert.equal(richlistitem.querySelector(".website-name > label")
                             .getAttribute("value"), expected[i][0]);
    Assert.equal(richlistitem.querySelector(".website-capability-value")
                             .getAttribute("data-l10n-id"), expected[i][1]);
  }
}

async function runTest(test, observances) {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.history.custom");
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let historyMode = doc.getElementById("historyMode");
  historyMode.value = "custom";
  historyMode.doCommand();

  let promiseSubDialogLoaded =
      promiseLoadSubDialog("chrome://browser/content/preferences/permissions.xul");
  doc.getElementById("cookieExceptions").doCommand();

  let win = await promiseSubDialogLoaded;

  doc = win.document;
  let params = {
    doc,
    richlistbox: doc.getElementById("permissionsBox"),
    url: doc.getElementById("url"),
    btnAllow: doc.getElementById("btnAllow"),
    btnBlock: doc.getElementById("btnBlock"),
    btnRemove: doc.getElementById("removePermission"),
    allowL10nId: win.gPermissionManager._getCapabilityL10nId(Ci.nsIPermissionManager.ALLOW_ACTION),
    denyL10nId: win.gPermissionManager._getCapabilityL10nId(Ci.nsIPermissionManager.DENY_ACTION),
    allow: Ci.nsIPermissionManager.ALLOW_ACTION,
    deny: Ci.nsIPermissionManager.DENY_ACTION,
  };
  let btnApplyChanges = doc.getElementById("btnApplyChanges");
  let observeAllPromise = createObserveAllPromise(observances);

  await test(params, observeAllPromise, () => btnApplyChanges.doCommand());

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

function createObserveAllPromise(observances) {
  return new Promise(resolve => {
    let permObserver = {
      observe(aSubject, aTopic, aData) {
        if (aTopic != "perm-changed")
          return;

        if (observances.length == 0) {
          // Should fail here as we are not expecting a notification, but we
          // don't. See bug 1063410.
          return;
        }

        info(`observed perm-changed (remaining ${observances.length - 1})`);

        let permission = aSubject.QueryInterface(Ci.nsIPermission);
        let expected = observances.shift();

        is(aData, expected.data, "type of message should be the same");
        for (let prop of ["type", "capability"]) {
          if (expected[prop])
            is(permission[prop], expected[prop],
               "property: \"" + prop + "\" should be equal");
        }

        if (expected.origin) {
          is(permission.principal.origin, expected.origin,
             "property: \"origin\" should be equal");
        }

        if (observances.length == 0) {
          Services.obs.removeObserver(permObserver, "perm-changed");
          executeSoon(resolve);
        }
      },
    };
    Services.obs.addObserver(permObserver, "perm-changed");
  });
}
