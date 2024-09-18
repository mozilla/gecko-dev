/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the ShutdownExceptionsCleaner
 */

"use strict";

// Test that only the cookie permission gets removed.
add_task(
  async function test_removing_shutdown_exception_permission_only_clears_cookie_permissions() {
    let uri = Services.io.newURI("https://example.net");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );

    // add "cookie" (== clear on shutdown exception) permission
    Services.perms.addFromPrincipal(
      principal,
      "cookie",
      Services.perms.ALLOW_ACTION
    );

    // add a different permission
    Services.perms.addFromPrincipal(
      principal,
      "notcookie",
      Services.perms.ALLOW_ACTION
    );

    await new Promise(aResolve => {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_SHUTDOWN_EXCEPTIONS,
        value => {
          Assert.equal(value, 0);
          aResolve();
        }
      );
    });

    // ensure "cookie" permission was cleared and other stays
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
      Services.perms.UNKNOWN_ACTION,
      "the cookie permission has been removed"
    );
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "notcookie"),
      Services.perms.ALLOW_ACTION,
      "the other permission has not been removed"
    );

    // reset permission manager
    await new Promise(aResolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        aResolve()
      );
    });

    Assert.equal(Services.perms.all.length, 0, "check all removed");
  }
);

// test that the CLEAR_SITE_PERMISSIONS flag does not remove the "cookie" permission
add_task(
  async function test_removing_site_permissions_skips_cookie_permissions() {
    let uri = Services.io.newURI("https://example.net");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );

    // add "cookie" (== clear on shutdown exception) permission
    Services.perms.addFromPrincipal(
      principal,
      "cookie",
      Services.perms.ALLOW_ACTION
    );

    // add a different permission
    Services.perms.addFromPrincipal(
      principal,
      "notcookie",
      Services.perms.ALLOW_ACTION
    );

    await new Promise(aResolve => {
      Services.clearData.deleteData(
        Ci.nsIClearDataService.CLEAR_SITE_PERMISSIONS,
        value => {
          Assert.equal(value, 0);
          aResolve();
        }
      );
    });

    // ensure "notcookie" permission was cleared and "cookie" stays
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
      Services.perms.ALLOW_ACTION,
      "the cookie permission has not been removed"
    );
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "notcookie"),
      Services.perms.UNKNOWN_ACTION,
      "the other permission has been removed"
    );

    // reset permission manager
    await new Promise(aResolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        aResolve()
      );
    });

    Assert.equal(Services.perms.all.length, 0, "check all removed");
  }
);

// test that the CLEAR_PERMISSIONS flag still removes both
add_task(async function test_removing_all_permissions() {
  let uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  // add "cookie" (== clear on shutdown exception) permission
  Services.perms.addFromPrincipal(
    principal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  // add a different permission
  Services.perms.addFromPrincipal(
    principal,
    "notcookie",
    Services.perms.ALLOW_ACTION
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  // ensure "notcookie" and "cookie" permission was cleared
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
    Services.perms.UNKNOWN_ACTION,
    "the cookie permission has been removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(principal, "notcookie"),
    Services.perms.UNKNOWN_ACTION,
    "the other permission has been removed"
  );

  // reset permission manager
  await new Promise(aResolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      aResolve()
    );
  });

  Assert.equal(Services.perms.all.length, 0, "check all removed");
});

// We can't test the pattern clearing here here since "cookie" permissions are
// not isolated by OA in the permission manager. See `kStripOAPermissions` in
// `PermissionManager.cpp`.
add_task(async function test_removeBySiteAndOAPattern() {
  let principalRegular = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://example.net"),
    {}
  );
  let principalRegularSub =
    Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI("https://sub.example.net"),
      {}
    );
  let principalUnrelated =
    Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI("https://example.org"),
      {}
    );

  info("add 'cookie' (== clear on shutdown exception) permissions");
  [principalRegular, principalRegularSub, principalUnrelated].forEach(
    principal => {
      Services.perms.addFromPrincipal(
        principal,
        "cookie",
        Services.perms.ALLOW_ACTION
      );
    }
  );

  info("Clear all permissions for schemeless site example.net");
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromSite(
      "example.net",
      {},
      true,
      Ci.nsIClearDataService.CLEAR_SHUTDOWN_EXCEPTIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  [principalRegular, principalRegularSub].forEach(principal => {
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
      Services.perms.UNKNOWN_ACTION,
      "cookie permission has been removed for " + principal.origin
    );
  });

  [principalUnrelated].forEach(principal => {
    Assert.equal(
      Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
      Services.perms.ALLOW_ACTION,
      "cookie permission still exists for " + principal.origin
    );
  });

  // reset permission manager
  await new Promise(aResolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      aResolve()
    );
  });

  Assert.equal(Services.perms.all.length, 0, "check all removed");
});
