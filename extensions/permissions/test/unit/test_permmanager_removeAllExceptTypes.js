/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function addPermissionsToManager() {
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

  Assert.equal(pm.all.length, 7, "Check all permissions were added");
}

add_task(function testRemoveAllWithSingleException() {
  // initialize the permission manager service
  let pm = Services.perms;

  // add permissions
  addPermissionsToManager();

  pm.removeAllExceptTypes(["apple"]);

  Assert.equal(
    pm.getAllByTypes(["apple"]).length,
    3,
    '"apple" type permissions should not be removed'
  );
  Assert.equal(
    pm.getAllByTypes(["pear", "cucumber"]).length,
    0,
    "All other permissions should be removed"
  );
  Assert.equal(pm.all.length, 3, "No other permissions should be present");

  pm.removeAllExceptTypes([]);
  Assert.equal(pm.all.length, 0, "All permissions should be removed");
});

add_task(function testRemoveAllWithMultipleExceptions() {
  // initialize the permission manager service
  let pm = Services.perms;

  // add permissions to permission manager
  addPermissionsToManager();

  pm.removeAllExceptTypes(["pear", "cucumber"]);

  Assert.equal(
    pm.getAllByTypes(["apple"]).length,
    0,
    'Should be no permissions with type "apple"'
  );
  Assert.equal(
    pm.getAllByTypes(["pear", "cucumber"]).length,
    4,
    "All non-apple permissions should not be removed"
  );
  Assert.equal(pm.all.length, 4, "No other permissions should be present");

  pm.removeAllExceptTypes([]);
  Assert.equal(pm.all.length, 0, "All permissions should be removed");
});
