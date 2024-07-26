/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function testRemovePermissionsByOriginAttributes() {
  // initialize the permission manager service
  let pm = Services.perms;

  Assert.equal(pm.all.length, 0, "Should start with no permissions");

  // add some permissions
  let attrs1 = {
    privateBrowsingId: 1,
  };
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://amazon.com:8080"),
    attrs1
  );

  let attrs2 = {
    partitionKey: "example.com",
    privateBrowsingId: 1,
  };
  let principal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://google.com:2048"),
    attrs2
  );

  let attrs3 = {
    privateBrowsingId: 0,
  };
  let principal3 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://google.com"),
    attrs3
  );

  let principal4 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://mozilla.org"),
    {}
  );

  pm.addFromPrincipal(principal, "type", Ci.nsIPermissionManager.ALLOW_ACTION);
  pm.addFromPrincipal(principal2, "type", Ci.nsIPermissionManager.ALLOW_ACTION);
  pm.addFromPrincipal(principal3, "type", Ci.nsIPermissionManager.ALLOW_ACTION);
  pm.addFromPrincipal(principal4, "type", Ci.nsIPermissionManager.ALLOW_ACTION);

  Assert.equal(pm.all.length, 4, "Check all permissions added");

  pm.removePermissionsWithAttributes(JSON.stringify(attrs1), [], []);

  // check exact match removed
  Assert.equal(
    pm.testPermissionFromPrincipal(
      principal,
      "type",
      "Check exact match removed"
    ),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  // check non-exact match removed
  Assert.equal(
    pm.testPermissionFromPrincipal(
      principal2,
      "type",
      "Check non-exact match removed"
    ),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );

  // check non-match not removed
  Assert.equal(
    pm.testPermissionFromPrincipal(
      principal3,
      "type",
      "Check non-match not removed"
    ),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  // check default originattributes (privateBrowsingId=0) not removed
  Assert.equal(
    pm.testPermissionFromPrincipal(
      principal4,
      "type",
      "Check default not removed"
    ),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  pm.removePermissionsWithAttributes(JSON.stringify({}), [], []);

  // check all removed
  Assert.equal(pm.all.length, 0, "All permissions should be removed");
});

add_task(function testRemovePermissionsByOriginAttributesWithExceptions() {
  // initialize the permission manager service
  let pm = Services.perms;

  Assert.equal(pm.all.length, 0, "Should start with no permissions");

  // add some permissions
  let attrs = { partitionKey: "mozilla.org" };
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://amazon.com:8080"),
    attrs
  );

  let principal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("http://google.com:2048"),
    attrs
  );

  let principal3 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://google.com"),
    {}
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
  pm.addFromPrincipal(
    principal,
    "watermelon",
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

  Assert.equal(pm.all.length, 8, "Check all permissions added");

  // remove all of type "apple" with originAttributes attrs
  pm.removePermissionsWithAttributes(JSON.stringify(attrs), ["apple"], []);
  Assert.equal(
    pm.all.length,
    6,
    '"apple" permissions with attr should be removed'
  );

  Assert.equal(pm.testPermissionFromPrincipal(principal, "apple"), 0);
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "pear"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "cucumber"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "watermelon"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "apple"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal2, "pear"),
    Ci.nsIPermissionManager.DENY_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "cucumber"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "apple"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  // remove rest except for perm with type "watermelon"
  pm.removePermissionsWithAttributes(JSON.stringify(attrs), [], ["watermelon"]);
  Assert.equal(
    pm.all.length,
    3,
    'All permissions with attrs should be removed except type "watermelon"'
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal, "watermelon"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "cucumber"),
    Ci.nsIPermissionManager.PROMPT_ACTION
  );
  Assert.equal(
    pm.testPermissionFromPrincipal(principal3, "apple"),
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  // remove all
  pm.removePermissionsWithAttributes(JSON.stringify({}), [], []);
  Assert.equal(pm.all.length, 0, "All permissions should be removed");
});
