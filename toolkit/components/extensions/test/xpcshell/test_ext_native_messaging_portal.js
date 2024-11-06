/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Subprocess: "resource://gre/modules/Subprocess.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

// Helpful documentation on the WebExtensions portal that is being tested here:
//   - feature request: https://github.com/flatpak/xdg-desktop-portal/issues/655
//   - pull request: https://github.com/flatpak/xdg-desktop-portal/pull/705
//   - D-Bus API: https://github.com/jhenstridge/xdg-desktop-portal/blob/native-messaging-portal/data/org.freedesktop.portal.WebExtensions.xml

const SESSION_HANDLE =
  "/org/freedesktop/portal/desktop/session/foobar/firefox_xpcshell_tests_mozilla_org_42";

const portalBusName = "org.freedesktop.portal.Desktop";
const portalObjectPath = "/org/freedesktop/portal/desktop";
const portalInterfaceName = "org.freedesktop.portal.WebExtensions";
const sessionInterfaceName = "org.freedesktop.portal.Session";
const dbusMockInterface = "org.freedesktop.DBus.Mock";
const addObjectMethod = `${dbusMockInterface}.AddObject`;
const addMethodMethod = `${dbusMockInterface}.AddMethod`;
const addPropertyMethod = `${dbusMockInterface}.AddProperty`;
const updatePropertiesMethod = `${dbusMockInterface}.UpdateProperties`;
const emitSignalDetailedMethod = `${dbusMockInterface}.EmitSignalDetailed`;
const getCallsMethod = `${dbusMockInterface}.GetCalls`;
const clearCallsMethod = `${dbusMockInterface}.ClearCalls`;
const resetMethod = `${dbusMockInterface}.Reset`;
const mockRequestObjectPath = "/org/freedesktop/portal/desktop/request";
const mockManifest =
  '{"name":"echo","description":"a native connector","type":"stdio","path":"/usr/bin/echo","allowed_extensions":["native@tests.mozilla.org"]}';
const nativeMessagingPref = "widget.use-xdg-desktop-portal.native-messaging";

var DBUS_SESSION_BUS_ADDRESS = "";
var DBUS_SESSION_BUS_PID = 0; // eslint-disable-line no-unused-vars
var DBUS_MOCK = null;
var FDS_MOCK = null;

async function background() {
  let port;
  browser.test.onMessage.addListener(async (what, payload) => {
    if (what == "request") {
      await browser.permissions.request({ permissions: ["nativeMessaging"] });
      // connectNative requires permission
      port = browser.runtime.connectNative("echo");
      port.onMessage.addListener(msg => {
        browser.test.sendMessage("message", msg);
      });
      browser.test.sendMessage("ready");
    } else if (what == "send") {
      if (payload._json) {
        let json = payload._json;
        payload.toJSON = () => json;
        delete payload._json;
      }
      port.postMessage(payload);
    }
  });
}

async function mockSetup(objectPath, methodName, args) {
  let mockProcess = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("gdbus"),
    arguments: [
      "call",
      "--session",
      "-d",
      portalBusName,
      "-o",
      objectPath,
      "-m",
      methodName,
      ...args,
    ],
  });
  return mockProcess.wait();
}

add_setup(async function () {
  // Start and use a separate message bus for the tests, to not interfere with
  // the current's session message bus.
  let dbus = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("dbus-launch"),
  });
  let stdout = await dbus.stdout.readString();
  let lines = stdout.split("\n");
  for (let i in lines) {
    let tokens = lines[i].split("=");
    switch (tokens.shift()) {
      case "DBUS_SESSION_BUS_ADDRESS":
        DBUS_SESSION_BUS_ADDRESS = tokens.join("=");
        break;
      case "DBUS_SESSION_BUS_PID":
        DBUS_SESSION_BUS_PID = tokens.join();
        break;
      default:
    }
  }

  let prefValue = Services.prefs.getIntPref(nativeMessagingPref, 0);
  Services.prefs.setIntPref(nativeMessagingPref, 2);

  Services.env.set("DBUS_SESSION_BUS_ADDRESS", DBUS_SESSION_BUS_ADDRESS);
  Services.env.set("GTK_USE_PORTAL", "1");

  // dbusmock is used to mock the native messaging portal's D-Bus API.
  DBUS_MOCK = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("python3"),
    arguments: [
      "-m",
      "dbusmock",
      portalBusName,
      portalObjectPath,
      portalInterfaceName,
    ],
  });

  // When talking to the native messaging portal over D-Bus, it returns a tuple
  // of file descriptors. For the mock to work correctly, the file descriptors
  // must exist, so create a dummy process in order to use its stdin, stdout
  // and stderr file descriptors.
  FDS_MOCK = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("tail"),
    arguments: ["-f", "/dev/null"],
    stderr: "pipe",
  });

  registerCleanupFunction(async function () {
    await FDS_MOCK.kill();
    await mockSetup(portalObjectPath, resetMethod, []);
    await DBUS_MOCK.kill();
    // XXX: While this works locally, it consistently fails when tests are run
    // in CI, with "xpcshell return code: -15". This needs to be investigated
    // further. This leaves a stray dbus-daemon process behind,
    // which isn't ideal, but is harmless.
    /*await lazy.Subprocess.call({
      command: await lazy.Subprocess.pathSearch("kill"),
      arguments: ["-SIGQUIT", DBUS_SESSION_BUS_PID],
    });*/
    Services.prefs.setIntPref(nativeMessagingPref, prefValue);
  });

  // Set up the mock objects and methods.
  await mockSetup(portalObjectPath, addPropertyMethod, [
    portalInterfaceName,
    "version",
    "<uint32 1>",
  ]);
  await mockSetup(portalObjectPath, addMethodMethod, [
    portalInterfaceName,
    "CreateSession",
    "a{sv}",
    "o",
    `ret = "${SESSION_HANDLE}"`,
  ]);
  await mockSetup(portalObjectPath, addObjectMethod, [
    SESSION_HANDLE,
    sessionInterfaceName,
    "@a{sv} {}",
    "@a(ssss) [('Close', '', '', '')]",
  ]);
  await mockSetup(portalObjectPath, addMethodMethod, [
    portalInterfaceName,
    "GetManifest",
    "oss",
    "s",
    `ret = '${mockManifest}'`,
  ]);
  await mockSetup(portalObjectPath, addMethodMethod, [
    portalInterfaceName,
    "Start",
    "ossa{sv}",
    "o",
    `ret = "${mockRequestObjectPath}/foobar"`,
  ]);
  await mockSetup(portalObjectPath, addMethodMethod, [
    portalInterfaceName,
    "GetPipes",
    "oa{sv}",
    "hhh",
    `ret = (dbus.types.UnixFd(${FDS_MOCK.stdin.fd}), dbus.types.UnixFd(${FDS_MOCK.stdout.fd}), dbus.types.UnixFd(${FDS_MOCK.stderr.fd}))`,
  ]);

  optionalPermissionsPromptHandler.init();
  optionalPermissionsPromptHandler.acceptPrompt = true;
  await AddonTestUtils.promiseStartupManager();
  await setupHosts([]); // these tests don't use any native app script
});

async function verifyDbusMockCall(objectPath, method, offset) {
  let getCalls = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("gdbus"),
    arguments: [
      "call",
      "--session",
      "-d",
      portalBusName,
      "-o",
      objectPath,
      "-m",
      getCallsMethod,
    ],
  });
  let out = await getCalls.stdout.readString();
  out = out.match(/\((@a\(tsav\) )?\[(.*)\],\)/)[2];
  let calls = out.matchAll(/\(.*?\),?/g);
  let methodCalled = false;
  let params = {};
  let i = 0;
  for (let call of calls) {
    if (i++ < offset) {
      continue;
    }
    let matches = call[0].match(
      /\((uint64 )?(?<timestamp>\d+), '(?<method>\w+)', (@av )?\[(?<params>.*)\]\),?/
    );
    ok(parseFloat(matches.groups.timestamp), "timestamp is valid");
    if (matches.groups.method == method) {
      methodCalled = true;
      params = matches.groups.params;
      break;
    }
  }
  if (method) {
    ok(methodCalled, `The ${method} mock was called`);
  } else {
    equal(i, 0, "No method mock was called");
  }
  return { offset: i, params: params };
}

add_task(async function test_talk_to_portal() {
  await mockSetup(portalObjectPath, clearCallsMethod, []);

  // Make sure the portal is considered available
  await mockSetup(portalObjectPath, updatePropertiesMethod, [
    portalInterfaceName,
    "{'version': <uint32 1>}",
  ]);

  // dbusmock's logging output doesn't reveal the sender name,
  // so run dbus-monitor in parallel. The sender name is needed to build the
  // object path of the Request that is returned by the portal's Start method.
  let dbusMonitor = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("dbus-monitor"),
    arguments: [
      "--session",
      `interface='${portalInterfaceName}', member='CreateSession'`,
    ],
  });

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      optional_permissions: ["nativeMessaging"],
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    await extension.awaitMessage("ready");
  });

  let handleToken = "";
  let senderName = "";

  // Verify that starting the extension talks to the mock native messaging
  // portal (i.e. CreateSession and Start are called with the expected
  // arguments).
  let result = await verifyDbusMockCall(portalObjectPath, "CreateSession", 0);
  result = await verifyDbusMockCall(
    portalObjectPath,
    "GetManifest",
    result.offset
  );
  result = await verifyDbusMockCall(portalObjectPath, "Start", result.offset);
  let match = result.params.match(/{'handle_token': <'(?<token>.*)'>}/);
  ok(match, "Start arguments contain a handle token");
  handleToken = match.groups.token;

  // Extract the sender name from the dbus-monitor process's output.
  let dbusMonitorOutput = await dbusMonitor.stdout.readString();
  let lines = dbusMonitorOutput.split("\n");
  for (let i in lines) {
    let line = lines[i];
    if (!line) {
      continue;
    }
    if (line.startsWith("method call")) {
      let match = line.match(/sender=(\S*)/);
      ok(match, "dbus-monitor output informs us of the sender");
      senderName = match[1];
    }
  }
  ok(senderName, "Got the sender name");
  await dbusMonitor.kill();

  // Mock the Request object that is expected to be created in response to
  // calling the Start method on the native messaging portal, wait for it to be
  // available, and emit its Response signal.
  let requestPath = `${mockRequestObjectPath}/${senderName
    .slice(1)
    .replace(".", "_")}/${handleToken}`;
  await mockSetup(portalObjectPath, addObjectMethod, [
    requestPath,
    "org.freedesktop.portal.Request",
    "@a{sv} {}",
    "@a(ssss) []",
  ]);
  let waitForRequestObject = await lazy.Subprocess.call({
    command: await lazy.Subprocess.pathSearch("gdbus"),
    arguments: [
      "introspect",
      "--session",
      "-d",
      portalBusName,
      "-o",
      requestPath,
      "-p",
    ],
  });
  await waitForRequestObject.wait();
  await mockSetup(requestPath, emitSignalDetailedMethod, [
    "org.freedesktop.portal.Request",
    "Response",
    "ua{sv}",
    "[<uint32 0>, <@a{sv} {}>]",
    `{'destination': <'${senderName}'>}`,
  ]);

  // Verify that the GetPipes method of the native messaging portal mock was
  // called as expected after the Start request completed.
  await verifyDbusMockCall(portalObjectPath, "GetPipes", result.offset);

  await extension.unload();

  // Verify that the native messaging portal session is properly closed when
  // the extension is unloaded.
  await verifyDbusMockCall(SESSION_HANDLE, "Close", 0);
});

add_task(async function test_portal_unavailable() {
  await mockSetup(portalObjectPath, clearCallsMethod, []);

  // Make sure the portal is NOT considered available
  await mockSetup(portalObjectPath, updatePropertiesMethod, [
    portalInterfaceName,
    "{'version': <uint32 0>}",
  ]);

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: ID } },
      optional_permissions: ["nativeMessaging"],
    },
    useAddonManager: "temporary",
  });

  let logged = false;
  function listener(msg) {
    logged ||= /Native messaging portal is not available/.test(msg.message);
  }
  Services.console.registerListener(listener);
  registerCleanupFunction(() => {
    Services.console.unregisterListener(listener);
  });

  await extension.startup();
  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    await extension.awaitMessage("ready");
  });

  ok(logged, "Non availability of the portal was logged");

  // Verify that the native messaging portal wasn't talked to,
  // because it advertised itself as not available.
  await verifyDbusMockCall(portalObjectPath, null, 0);

  await extension.unload();
});
