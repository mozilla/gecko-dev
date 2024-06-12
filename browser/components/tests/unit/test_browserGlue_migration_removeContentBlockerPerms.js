/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_BROWSERGLUE_TEST = "force-ui-migration";
const UI_VERSION = 147;
const CONTENT_BLOCKER_PERM_TYPES = [
  "other",
  "script",
  "image",
  "stylesheet",
  "object",
  "document",
  "subdocument",
  "refresh",
  "xbl",
  "ping",
  "xmlhttprequest",
  "objectsubrequest",
  "dtd",
  "font",
  "websocket",
  "csp_report",
  "xslt",
  "beacon",
  "fetch",
  "manifest",
  "speculative",
];

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

// Test to check if migration resets default permissions properly.
add_task(async function test_removeContentBlockerPerms() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.migration.version");
    Services.perms.removeAll();
  });

  Services.perms.removeAll();
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION);

  let pm = Services.perms;

  // Add a permission for each nsContentBlocker type
  CONTENT_BLOCKER_PERM_TYPES.forEach(type => {
    pm.addFromPrincipal(
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "https://www.mozilla.org"
      ),
      type,
      pm.ALLOW_ACTION
    );
  });

  // Check all permissions were added
  let remaining_perms = pm.getAllByTypes(CONTENT_BLOCKER_PERM_TYPES);
  Assert.equal(
    remaining_perms.length,
    CONTENT_BLOCKER_PERM_TYPES.length,
    `Number permissions added vs. expected`
  );

  // Check default permissions are present
  let nonCBPerms = pm.all.length - CONTENT_BLOCKER_PERM_TYPES.length;
  Assert.greater(
    nonCBPerms,
    0,
    "Check at least one non-content blocker permission is present"
  );

  // Simulate a migration.
  gBrowserGlue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_BROWSERGLUE_TEST
  );

  // Check all permissions were deleted
  remaining_perms = pm.getAllByTypes(CONTENT_BLOCKER_PERM_TYPES);
  Assert.equal(
    remaining_perms.length,
    0,
    `All content blocker permissions should be deleted`
  );

  // Check rest of permissions were not deleted
  Assert.equal(
    pm.all.length,
    nonCBPerms,
    "Default permissions should not be deleted"
  );
});
