/* Requires Ubuntu 22.04 and higher, depends on https://launchpad.net/bugs/1968215
 * Future Ubuntu versions may depend on xdg-native-messaging-proxy instead,
 * see https://bugzilla.mozilla.org/show_bug.cgi?id=1955255
 */
"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Invalid process ID/);

const NATIVE_APP_BASENAME = "pingpong_nativemessaging_test";
/*
 * Write native application manifest to $HOME/.mozilla/native-messaging-hosts
 * We write it there because that is where the xdg-desktop-portal looks and we
 * are supposedly running this inside a sandbox (snap or flatpack).
 */
const NATIVE_APP_PATH = do_get_file(
  "data/pingpong_nativemessaging_test.py"
).path;
info(`This test will launch ${NATIVE_APP_PATH}`);

/*
 * This extension ID cannot be registered by regular users
 * because the addons.mozilla.org website won't sign such extensions.
 *
 * This is important, to make sure that nobody can trick a Mozilla developer
 * into installing an extension and then abusing the functionality exposed in
 * the native messaging host from this test.
 */
const EXTENSION_ID = "native-messaging-portal-test@tests.mozilla.org";
let pingpong_manifest = `
{
  "name": "${NATIVE_APP_BASENAME}",
  "description": "Example host for native messaging",
  "path": "${NATIVE_APP_PATH}",
  "type": "stdio",
  "allowed_extensions": [ "${EXTENSION_ID}" ]
}
`;

add_setup(async function setup() {
  // This environment variable must be enabled
  Assert.equal(
    Services.env.get("GTK_USE_PORTAL"),
    1,
    "GTK_USE_PORTAL needs to be set."
  );

  // Change from default 0 (portal disabled) to 2 (auto detect).
  Services.prefs.setIntPref(
    "widget.use-xdg-desktop-portal.native-messaging",
    2
  );

  /* Place a GRANT permission in the Permission Store so that the portal can
   * directly retrieve a YES instead of prompting for permission (which would
   * require user interaction).
   */
  let p = await Subprocess.call({
    command: await Subprocess.pathSearch("dbus-send"),
    arguments: [
      "--session",
      "--type=method_call",
      "--dest=org.freedesktop.impl.portal.PermissionStore",
      "/org/freedesktop/impl/portal/PermissionStore",
      "org.freedesktop.impl.portal.PermissionStore.SetPermission",
      "string:webextensions",
      "boolean:true",
      "string:pingpong_nativemessaging_test",
      "string:",
      "array:string:yes",
    ],
  });
  let { exitcode } = await p.wait();
  Assert.equal(
    exitcode,
    undefined,
    "Dbus call to allow the test native messaging app should succeed"
  );

  /* Remove said GRANT permission after the test is done. */
  registerCleanupFunction(async function () {
    let y = await Subprocess.call({
      command: await Subprocess.pathSearch("dbus-send"),
      arguments: [
        "--session",
        "--type=method_call",
        "--print-reply",
        "--dest=org.freedesktop.impl.portal.PermissionStore",
        "/org/freedesktop/impl/portal/PermissionStore",
        "org.freedesktop.impl.portal.PermissionStore.DeletePermission",
        "string:webextensions",
        "string:pingpong_nativemessaging_test",
        "string:",
      ],
    });
    let { exitcode } = await y.wait();
    Assert.equal(
      exitcode,
      undefined,
      "Clean-up of store permission should succeed."
    );
  });

  /* Although this test doesn't use any native app script,
   * head_native_messaging.js relies on dirsvc, which this line sets up. */
  await setupHosts([]);

  /* Write to $HOME/.mozilla/native-messaging-hosts/ because that is the
   * location with the highest precedence, where native messaging hosts should
   * be looked up. Firefox looks up the path from XREUserNativeManifests, which
   * effectively maps to that path on regular releases of Firefox. But in this
   * test, we want the portal to look up the path, which is independent of
   * Firefox, and therefore the value of XREUserNativeManifests is meaningless.
   * Therefore, we write directly to ~/.mozilla/native-messaging-hosts/.
   *
   * Side note: in reality, Firefox should not even be able to write to that
   * path when inside a sandbox (snap or flatpak). In this test, the xpcshell
   * runtime (which shares the same process as the parts of Firefox that is
   * being tested) is not actually being run in a real snap/flatpak. We merely
   * verify that the portal works.
   */
  let home = Services.env.get("HOME");
  Assert.notEqual(home, "", "HOME is set");
  /* This path should match XREUserNativeManifests, which is already verified
   * by test_force_legacy in
   * toolkit/components/extensions/test/xpcshell/test_native_messaging_paths_linux.js.
   */
  let pingpong_manifest_file =
    home + `/.mozilla/native-messaging-hosts/${NATIVE_APP_BASENAME}.json`;
  await IOUtils.writeUTF8(pingpong_manifest_file, pingpong_manifest, {
    flush: true,
  });
  registerCleanupFunction(async function () {
    await IOUtils.remove(pingpong_manifest_file);
  });
});

async function background() {
  try {
    let res = await browser.runtime.sendNativeMessage(
      "pingpong_nativemessaging_test",
      "ping"
    );
    browser.test.assertEq(
      "native app sends pong",
      res,
      "Expected reply from native messaging host"
    );
    await browser.test.assertRejects(
      browser.runtime.sendNativeMessage(
        "non_existent_nativemessaging_host",
        "ping"
      ),
      "No such native application non_existent_nativemessaging_host",
      "Expected error for non-existing native messaging host"
    );
  } catch (e) {
    browser.test.fail(`Unexpected error: ${e}`);
  }
  browser.test.sendMessage("done");
}

add_task(async function test_talk_to_native_application() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
      permissions: ["nativeMessaging"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
