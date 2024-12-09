/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

const COLLECTION_NAME = "remote-permissions";
const ORIGIN_1 = "https://example.com";
const PRINCIPAL_1 = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_1),
  {}
);
const PRINCIPAL_1_PB = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_1),
  { privateBrowsingId: 1 }
);
const ORIGIN_2 = "https://example.org";
const PRINCIPAL_2 = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_2),
  {}
);
const PRINCIPAL_2_PB = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(ORIGIN_2),
  { privateBrowsingId: 1 }
);
const ORIGIN_INVALID = "not a valid origin";
const TEST_PERMISSION_1 = "test-permission-1";
const TEST_PERMISSION_2 = "test-permission-2";

let rs = RemoteSettings(COLLECTION_NAME);
let pm = Services.perms;
let rps = Cc["@mozilla.org/remote-permission-service;1"].getService(
  Ci.nsIRemotePermissionService
);

async function remoteSettingsSync({ created, updated, deleted }) {
  await rs.emit("sync", {
    data: {
      created,
      updated,
      deleted,
    },
  });
}

function expectPermissions(perms) {
  Assert.deepEqual(
    pm.all
      .map(({ principal, type, capability }) => ({
        principal: principal.siteOrigin,
        type,
        capability,
      }))
      .sort((a, b) => a.principal.localeCompare(b.principal)),
    perms
      .map(({ principal, type, capability }) => ({
        principal: principal.siteOrigin,
        type,
        capability,
      }))
      .sort((a, b) => a.principal.localeCompare(b.principal)),
    "Permission manager should have expected permissions"
  );
}

add_setup(async function () {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");
  do_get_profile();

  // This needs to be restored on cleanup
  let originalPermissionValues = structuredClone(
    rps.testAllowedPermissionValues
  );

  // Initialize remote permission service
  Services.prefs.setBoolPref("permissions.manager.remote.enabled", true);
  let permObserver = Services.perms.QueryInterface(Ci.nsIObserver);
  permObserver.observe(null, "profile-after-change", "");
  await rps.isInitialized;

  registerCleanupFunction(async () => {
    info("Cleaning up");
    rps.testAllowedPermissionValues = originalPermissionValues;
    Services.prefs.clearUserPref("permissions.manager.defaultsUrl");
    Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
    Services.perms.removeAll();
  });

  // Allow setting everything
  rps.testAllowedPermissionValues = {
    "*": ["*"],
  };
});

add_task(async function test_create_permission() {
  info("Creating permission");

  await remoteSettingsSync({
    created: [
      {
        origin: ORIGIN_1,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
      },
    ],
  });

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
  ]);
});

add_task(async function test_update_permission_value() {
  info("Updating permission value");

  await remoteSettingsSync({
    updated: [
      {
        old: {
          origin: ORIGIN_1,
          type: TEST_PERMISSION_1,
          capability: Ci.nsIPermissionManager.ALLOW_ACTION,
        },
        new: {
          origin: ORIGIN_1,
          type: TEST_PERMISSION_1,
          capability: Ci.nsIPermissionManager.DENY_ACTION,
        },
      },
    ],
  });

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);
});

add_task(async function test_update_permission_origin() {
  info("Updating permission origin");

  await remoteSettingsSync({
    updated: [
      {
        old: {
          origin: ORIGIN_1,
          type: TEST_PERMISSION_1,
          capability: Ci.nsIPermissionManager.DENY_ACTION,
        },
        new: {
          origin: ORIGIN_2,
          type: TEST_PERMISSION_1,
          capability: Ci.nsIPermissionManager.DENY_ACTION,
        },
      },
    ],
  });

  expectPermissions([
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);
});

add_task(async function test_user_permission_restoration() {
  info("Overriding with user permission");

  pm.addFromPrincipal(
    PRINCIPAL_2,
    TEST_PERMISSION_1,
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  expectPermissions([
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);

  info("Removing user permission");

  pm.removeFromPrincipal(PRINCIPAL_2, TEST_PERMISSION_1);

  expectPermissions([
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);
});

add_task(async function test_remove_all_restoration() {
  info("Overriding with user permission and adding new user permission");

  pm.addFromPrincipal(
    PRINCIPAL_1,
    TEST_PERMISSION_1,
    Ci.nsIPermissionManager.ALLOW_ACTION
  );
  pm.addFromPrincipal(
    PRINCIPAL_2,
    TEST_PERMISSION_1,
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);

  info("Removing all permissions");

  pm.removeAll();

  expectPermissions([
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);
});

add_task(async function test_delete_permission() {
  info("Deleting permission");

  await remoteSettingsSync({
    deleted: [
      {
        origin: ORIGIN_2,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.DENY_ACTION,
      },
    ],
  });

  expectPermissions([]);
});

add_task(async function test_allowlist() {
  info("Only allowing TEST_PERMISSION_1 with value ALLOW_ACTION");

  rps.testAllowedPermissionValues = {
    [TEST_PERMISSION_1]: [Ci.nsIPermissionManager.ALLOW_ACTION],
  };

  info("Trying to add all sorts of default permissions");

  await remoteSettingsSync({
    created: [ORIGIN_1, ORIGIN_2].flatMap(origin =>
      [TEST_PERMISSION_1, TEST_PERMISSION_2].flatMap(type =>
        [
          Ci.nsIPermissionManager.ALLOW_ACTION,
          Ci.nsIPermissionManager.DENY_ACTION,
          Ci.nsIPermissionManager.PROMPT_ACTION,
        ].flatMap(capability => ({ origin, type, capability }))
      )
    ),
  });

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_2,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_2_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
  ]);

  rps.testAllowedPermissionValues = {
    "*": ["*"],
  };
});

add_task(async function test_defaults_url() {
  info("Testing interaction with permissions.manager.defaultsUrl");

  info("Setting up permissions.manager.defaultsUrl");

  let file = do_get_tempdir();
  file.append("test_default_permissions");
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(file, -1, 0o666, 0);
  let conv = Cc["@mozilla.org/intl/converter-output-stream;1"].createInstance(
    Ci.nsIConverterOutputStream
  );
  conv.init(ostream, "UTF-8");
  conv.writeString(
    [
      "host",
      TEST_PERMISSION_1,
      Ci.nsIPermissionManager.ALLOW_ACTION,
      ORIGIN_1,
    ].join("\t") + "\n"
  );
  conv.writeString(
    [
      "host",
      TEST_PERMISSION_2,
      Ci.nsIPermissionManager.ALLOW_ACTION,
      ORIGIN_1,
    ].join("\t") + "\n"
  );
  ostream.close();

  Services.prefs.setCharPref(
    "permissions.manager.defaultsUrl",
    "file://" + file.path
  );

  info("Re-initializing permission manager");

  // Start from a clean slate with our new default permissions from
  // permissions.manager.defaultsUrl
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
  Services.perms.removeAll();

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_2,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_2,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
  ]);

  info("Overriding permissions from permissions.manager.defaultsUrl");

  await remoteSettingsSync({
    created: [
      {
        origin: ORIGIN_1,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.DENY_ACTION,
      },
      {
        origin: ORIGIN_1,
        type: TEST_PERMISSION_2,
        capability: Ci.nsIPermissionManager.UNKNOWN_ACTION,
      },
    ],
  });

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.DENY_ACTION,
    },
  ]);
});

add_task(async function test_malformed_origin() {
  info(
    "Testing that import will continue after encountering a malformed origin"
  );

  await remoteSettingsSync({
    created: [
      {
        origin: ORIGIN_INVALID,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
      },
      // TEST_PERMISSION_1 still exists for ORIGIN_1 from the previous step, but
      // for simplicity we act like it is new here. We will see if the value has
      // changed from deny to allow.
      {
        origin: ORIGIN_1,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
      },
    ],
  });

  expectPermissions([
    {
      principal: PRINCIPAL_1,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
    {
      principal: PRINCIPAL_1_PB,
      type: TEST_PERMISSION_1,
      capability: Ci.nsIPermissionManager.ALLOW_ACTION,
    },
  ]);
});
