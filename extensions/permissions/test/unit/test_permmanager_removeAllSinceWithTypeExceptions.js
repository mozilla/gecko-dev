/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function testRemovePermissionsSinceATimeWithTypeExceptions() {
  // initialize the permission manager service
  let pm = Services.perms;

  Assert.equal(pm.all.length, 0, "Should start with no permissions");

  // add some permissions
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://amazon.com:8080"
    );
  let principal2 =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://google.com:2048"
    );
  let principal3 =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://google.com"
    );

  pm.addFromPrincipal(
    principal,
    "apple",
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  pm.addFromPrincipal(principal, "pear", Ci.nsIPermissionManager.ALLOW_ACTION);
  pm.addFromPrincipal(
    principal,
    "cucumber",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  // sleep briefly, then record the time - we'll remove some permissions since then.
  await new Promise(resolve => do_timeout(20, resolve));

  let since = Date.now();

  // from test_permmanager_removebytypesince.js:
  // *sob* - on Windows at least, the now recorded by PermissionManager.cpp
  // might be a couple of ms *earlier* than what JS sees.  So another sleep
  // to ensure our |since| is greater than the time of the permissions we
  // are now adding.  Sadly this means we'll never be able to test when since
  // exactly equals the modTime, but there you go...
  await new Promise(resolve => do_timeout(20, resolve));

  pm.addFromPrincipal(principal2, "apple", Ci.nsIPermissionManager.DENY_ACTION);
  pm.addFromPrincipal(principal2, "pear", Ci.nsIPermissionManager.DENY_ACTION);

  pm.addFromPrincipal(
    principal3,
    "cucumber",
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  pm.addFromPrincipal(
    principal3,
    "apple",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  Assert.equal(pm.all.length, 7, "Check all permissions added");

  // remove with multiple type exceptions
  pm.removeAllSinceWithTypeExceptions(since, ["pear", "cucumber"]);

  Assert.equal(
    pm.all.length,
    5,
    '"apple" permissions added after since should be removed'
  );

  // check exact permissions
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "pear"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "pear"),
    Ci.nsIPermissionManager.DENY_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "apple"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "cucumber"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "cucumber"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );

  // remove with a single type exception
  pm.removeAllSinceWithTypeExceptions(since, ["pear"]);
  Assert.equal(
    pm.all.length,
    4,
    '"cucumber" permission added after since should be removed'
  );

  // check exact permissions
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "pear"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "pear"),
    Ci.nsIPermissionManager.DENY_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "apple"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "cucumber"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "cucumber"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  pm.removeAllSinceWithTypeExceptions(since, []);
  Assert.equal(
    pm.all.length,
    3,
    "All permissions added after since should be removed"
  );

  // check exact permissions
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "pear"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "pear"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "apple"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "cucumber"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "cucumber"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  pm.removeAll();
  Assert.equal(pm.all.length, 0, "All permissions should be removed");
});
