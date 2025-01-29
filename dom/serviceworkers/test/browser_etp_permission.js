"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const BASE_URI = "http://mochi.test:8888/browser/dom/serviceworkers/test/";
const PAGE_URI = BASE_URI + "empty.html";
const SCOPE = PAGE_URI + "?etp_permission";
const SW_SCRIPT = BASE_URI + "service_worker_etp.js";

// Test that a service worker respects ETP permissions
// during ServiceWorkerPrivate::Initialize.
add_task(async function test_permission_during_init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      [
        "privacy.fingerprintingProtection.overrides",
        "+CanvasImageExtractionPrompt",
      ],
    ],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const checkAllowList = async expected => {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ script: SW_SCRIPT, scope: SCOPE, expected }],
      async function (opts) {
        const reg = await content.navigator.serviceWorker.register(
          opts.script,
          {
            scope: opts.scope,
          }
        );
        const worker = reg.installing || reg.waiting || reg.active;
        await new Promise(resolve => {
          if (worker.state === "activated") {
            resolve();
            return;
          }
          worker.addEventListener("statechange", function onStateChange() {
            if (worker.state === "activated") {
              worker.removeEventListener("statechange", onStateChange);
              resolve();
            }
          });
        });

        worker.postMessage("IsETPAllowListed");
        const response = await new Promise(resolve => {
          content.navigator.serviceWorker.onmessage = function (e) {
            resolve(e.data);
          };
        });

        is(
          response,
          opts.expected,
          `Service worker should ${!opts.expected ? "not" : ""} be allow-listed`
        );

        await reg.unregister();
      }
    );
  };

  await checkAllowList(false);

  PermissionTestUtils.add(
    SCOPE,
    "trackingprotection",
    Services.perms.ALLOW_ACTION
  );

  await checkAllowList(true);

  PermissionTestUtils.remove(SCOPE, "trackingprotection");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Test that a service worker respects ETP permissions
// during service worker runtime.
add_task(async function test_permission_during_runtime() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      [
        "privacy.fingerprintingProtection.overrides",
        "+CanvasImageExtractionPrompt",
      ],
    ],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Define functions with nice names for easier understanding
  const registerServiceWorker = async () => {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ script: SW_SCRIPT, scope: SCOPE }],
      async function (opts) {
        const reg = await content.navigator.serviceWorker.register(
          opts.script,
          {
            scope: opts.scope,
          }
        );
        const worker = reg.installing || reg.waiting || reg.active;
        await new Promise(resolve => {
          if (worker.state === "activated") {
            resolve();
            return;
          }
          worker.addEventListener("statechange", function onStateChange() {
            if (worker.state === "activated") {
              worker.removeEventListener("statechange", onStateChange);
              resolve();
            }
          });
        });
      }
    );
  };

  const checkAllowList = async expected => {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ expected }],
      async function (opts) {
        content.navigator.serviceWorker.ready.then(registration => {
          registration.active.postMessage("IsETPAllowListed");
        });

        const response = await new Promise(resolve => {
          content.navigator.serviceWorker.onmessage = function (e) {
            resolve(e.data);
          };
        });

        is(
          response,
          opts.expected,
          `Service worker should ${!opts.expected ? "not" : ""} be allow-listed`
        );
      }
    );
  };

  const unregisterServiceWorker = async () => {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ scope: SCOPE }],
      async function (opts) {
        const registrations =
          await content.navigator.serviceWorker.getRegistrations();
        for (const registration of registrations) {
          if (registration.scope === opts.scope) {
            await registration.unregister();
          }
        }
      }
    );
  };

  // Run the actual test
  await registerServiceWorker();

  await checkAllowList(false);

  PermissionTestUtils.add(
    SCOPE,
    "trackingprotection",
    Services.perms.ALLOW_ACTION
  );

  await checkAllowList(true);

  PermissionTestUtils.add(
    SCOPE,
    "trackingprotection",
    Services.perms.DENY_ACTION
  );

  await checkAllowList(false);

  await unregisterServiceWorker();

  PermissionTestUtils.remove(SCOPE, "trackingprotection");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
