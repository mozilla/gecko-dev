Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);

async function setAutograntPreferences() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 0],
    ],
  });
}

add_task(async function testPopupWithUserInteraction() {
  await setPreferences();
  await setAutograntPreferences();

  // Test that requesting storage access initially does not autogrant.
  // If the autogrant doesn't occur, we click reject on the door hanger
  // and expect the promise returned by requestStorageAccess to reject.
  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );

  // Grant the fedCM permissions to the first party.
  // This signifies that it should have a free access to the storage access
  // permission
  const uri = Services.io.newURI(TEST_TOP_PAGE);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  Services.perms.addFromPrincipal(
    principal,
    "credential-allow-silent-access",
    Services.perms.ALLOW_ACTION
  );
  const idpURI = Services.io.newURI(TEST_3RD_PARTY_PAGE);
  const idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    idpURI,
    {}
  );
  Services.perms.addFromPrincipal(
    principal,
    "credential-allow-silent-access^" + idpPrincipal.origin,
    Services.perms.ALLOW_ACTION
  );

  // Test that requesting storage access autogrants here. If a popup occurs,
  // expectNoPopup will cause an error in this test.
  await openPageAndRunCode(
    TEST_TOP_PAGE,
    expectNoPopup,
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectSuccess
  );

  let permissionValueAfterTest = Services.perms.testPermissionFromPrincipal(
    principal,
    "3rdPartyFrameStorage^" + idpPrincipal.siteOrigin
  );

  is(
    permissionValueAfterTest,
    Services.perms.ALLOW_ACTION,
    "Storage access permission was granted during the test."
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
