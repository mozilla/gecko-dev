/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Regression test for: https://bugzilla.mozilla.org/show_bug.cgi?id=1970075
//
// This test verifies that iconUrl as passed to browser.notifications.create()
// can be loaded. By default, the system backend is enabled, for which we can
// do little more than verifying that the options are set, but in case the
// system backend is disabled, we can verify that the image is actually loaded,
// because in this case Firefox is responsible for rendering the notification.

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const serverHost = server.identity.primaryHost;
const serverPort = server.identity.primaryPort;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["alerts.useSystemBackend", false]],
  });
});

async function testCreateNotification({ iconUrl, testOnShown }) {
  function background() {
    function createBlobUrlForTest() {
      const imgData = Uint8Array.fromBase64(
        // PNG image of size 5x5. test_blob_icon will verify the width.
        "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC"
      );
      const blob = new Blob([imgData], { type: "image/png" });
      return URL.createObjectURL(blob);
    }
    browser.test.onMessage.addListener(async (msg, iconUrl) => {
      browser.test.assertEq("iconUrl", msg, "Expected message");

      if (iconUrl == "blob:REPLACE_WITH_REAL_URL_IN_TEST") {
        iconUrl = createBlobUrlForTest();
      }
      if (iconUrl === "moz-extension:REPLACE_WITH_REAL_URL_IN_TEST") {
        iconUrl = browser.runtime.getURL("5x5.png");
      }

      let shownPromise = new Promise(resolve => {
        browser.notifications.onShown.addListener(resolve);
      });
      let closedPromise = new Promise(resolve => {
        browser.notifications.onClosed.addListener(resolve);
      });
      let createdId = await browser.notifications.create("notifid", {
        iconUrl,
        type: "basic",
        title: "title",
        message: "msg",
      });
      let shownId = await shownPromise;
      browser.test.assertEq(createdId, shownId, "ID of shown notification");
      browser.test.sendMessage("notification_shown");
      let closedId = await closedPromise;
      browser.test.assertEq(createdId, closedId, "ID of closed notification");
      browser.test.assertEq(
        "{}",
        JSON.stringify(await browser.notifications.getAll()),
        "no notifications left"
      );
      browser.test.sendMessage("notification_closed");
    });
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["notifications"],
    },
    background,
    files: {
      "5x5.png": imageBufferFromDataURI(
        "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC"
      ),
    },
  });
  await extension.startup();
  extension.sendMessage("iconUrl", iconUrl);
  await extension.awaitMessage("notification_shown");
  let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
  ok(alertWindow, "Found alert.xhtml window");
  await testOnShown(alertWindow);
  info("Closing alert.xhtml window");
  alertWindow.document.querySelector(".close-icon").click();
  await extension.awaitMessage("notification_closed");
  await extension.unload();
}

// Ideally we'd also repeat the following test for https, but the test server
// does not support https (bug 1742061).
add_task(async function test_http_icon() {
  const requestPromise = new Promise(resolve => {
    let count = 0;
    server.registerPathHandler("/test_http_icon.png", () => {
      // We only care about the request happening, we don't care about the
      // actual response.
      is(++count, 1, "Got one request to test_http_icon.png");
      resolve();
    });
  });

  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const httpUrl = `http://${serverHost}:${serverPort}/test_http_icon.png`;

  await testCreateNotification({
    iconUrl: httpUrl,
    async testOnShown(alertWindow) {
      info("Waiting for test_http_icon.png request to be detected.");
      const img = alertWindow.document.getElementById("alertImage");
      is(img.src, httpUrl, "Got http:-URL");
      await requestPromise;
    },
  });
});

add_task(async function test_data_icon() {
  // data-URL with a valid 5x5 image.
  const dataUrl =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC";

  await testCreateNotification({
    iconUrl: dataUrl,
    async testOnShown(alertWindow) {
      const img = alertWindow.document.getElementById("alertImage");
      is(img.src, dataUrl, "Got data:-URL");

      info("Verifying that data:-URL can be loaded in the document.");
      // img is not an <img> but an <image> element, so we cannot read its
      // intrinsic size directly to guess whether it was loaded.
      // To see whether it is NOT blocked by CSP, create a new image and see if
      // it can be loaded.

      const testImg = alertWindow.document.createElement("img");
      testImg.src = dataUrl;
      await testImg.decode();
      is(testImg.naturalWidth, 5, "Test image was loaded successfully");
    },
  });
});

add_task(async function test_blob_icon() {
  await testCreateNotification({
    iconUrl: "blob:REPLACE_WITH_REAL_URL_IN_TEST",
    async testOnShown(alertWindow) {
      const img = alertWindow.document.getElementById("alertImage");
      ok(img.src.startsWith("blob:moz-extension"), `Got blob:-URL: ${img.src}`);

      info("Verifying that blob:-URL can be loaded in the document.");

      const testImg = alertWindow.document.createElement("img");
      testImg.src = img.src;
      await testImg.decode();
      // The 5 here is the size of the test image, see createBlobUrlForTest.
      is(testImg.naturalWidth, 5, "Test image was loaded successfully");
    },
  });
});

add_task(async function test_moz_extension_icon() {
  await testCreateNotification({
    iconUrl: "moz-extension:REPLACE_WITH_REAL_URL_IN_TEST",
    async testOnShown(alertWindow) {
      const img = alertWindow.document.getElementById("alertImage");
      ok(
        img.src.startsWith("moz-extension:/") && img.src.endsWith("/5x5.png"),
        `Got moz-extension:-URL: ${img.src}`
      );

      info("Verifying that moz-extension:-URL can be loaded in the document.");

      const testImg = alertWindow.document.createElement("img");
      testImg.src = img.src;
      await testImg.decode();
      // The 5 here is the size of the test image (5x5.png).
      is(testImg.naturalWidth, 5, "Test image was loaded successfully");
    },
  });
});
