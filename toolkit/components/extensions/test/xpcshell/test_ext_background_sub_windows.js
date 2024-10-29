/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function testBackgroundWindow() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.log("background script executed");

      browser.test.sendMessage("background-script-load");

      let img = document.createElement("img");
      img.src =
        "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
      document.body.appendChild(img);

      img.onload = () => {
        browser.test.log("image loaded");

        let iframe = document.createElement("iframe");
        iframe.src = "about:blank?1";

        iframe.onload = () => {
          browser.test.log("iframe loaded");
          setTimeout(() => {
            browser.test.notifyPass("background sub-window test done");
          }, 0);
        };
        document.body.appendChild(iframe);
      };
    },
  });

  let loadCount = 0;
  extension.onMessage("background-script-load", () => {
    loadCount++;
  });

  await extension.startup();

  await extension.awaitFinish("background sub-window test done");

  equal(loadCount, 1, "background script loaded only once");

  await extension.unload();
});

add_task(async function testBackgroundMozExtSubframe() {
  const id = "bg-subframe@test-addon";
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      background: { scripts: ["bg.js"] },
      browser_specific_settings: { gecko: { id } },
    },
    files: {
      "bg.js": function () {
        const subframe = document.createElement("iframe");
        subframe.src = "iframe.html";
        subframe.onload = () =>
          browser.test.sendMessage("frame-load-result", {
            type: "load",
            url: subframe.contentWindow.location.href,
          });
        subframe.onerror = () =>
          browser.test.sendMessage("frame-load-result", {
            type: "error",
            url: subframe.contentWindow.location.href,
          });
        document.body.append(subframe);
      },
      "iframe.html": `<!DOCTYPE html><html><body>subframe</body></html>`,
      "extpage.html": `<!DOCTYPE html><html><body>extpage</body></html>`,
    },
  });

  info(
    "Test bgpage moz-extension subframe in addon temporarily installed as packed"
  );
  let extPacked = ExtensionTestUtils.expectExtension(id);
  await AddonManager.installTemporaryAddon(xpi);
  await extPacked.awaitStartup();
  Assert.deepEqual(
    await extPacked.awaitMessage("frame-load-result"),
    {
      type: "load",
      url: `moz-extension://${extPacked.uuid}/iframe.html`,
    },
    "Expect moz-extension subframe to be loaded successfully in zipped extension"
  );
  await extPacked.unload();

  // Repeat the test case again on an unpacked extension, which is
  // where Bug 1926106 was actually originally hit.
  info(
    "Test bgpage moz-extension subframe in addon temporarily installed as unpacked"
  );
  // This temporary directory is going to be removed from the
  // cleanup function, but also make it unique as we do for the
  // other temporary files (e.g. like getTemporaryFile as defined
  // in XPIInstall.sys.mjs).
  const random = Math.round(Math.random() * 36 ** 3).toString(36);
  const tmpDirName = `mochitest_unpacked_addons_${random}`;
  let tmpExtPath = FileUtils.getDir("TmpD", [tmpDirName]);
  tmpExtPath.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  registerCleanupFunction(() => {
    tmpExtPath.remove(true);
  });
  // Unpacking the xpi file into the temporary directory.
  const extDir = await AddonTestUtils.manuallyInstall(
    xpi,
    tmpExtPath,
    null,
    /* unpacked */ true
  );
  let extUnpacked = ExtensionTestUtils.expectExtension(id);
  await AddonManager.installTemporaryAddon(extDir);
  await extUnpacked.awaitStartup();
  Assert.deepEqual(
    await extUnpacked.awaitMessage("frame-load-result"),
    {
      type: "load",
      url: `moz-extension://${extUnpacked.uuid}/iframe.html`,
    },
    "Expect moz-extension subframe to be loaded successfully in unpacked extension"
  );

  // This is the opposite case where the nested frame url is one that
  // we wouldn't find on disk (instead of the parent frame), that isn't
  // an actual scenario that developers should hit, but it would ensure
  // that scenario works in the short term (and it may become useful if
  // we would get another generated moz-extension page that would make
  // sense for developers to use as a subframe url in practice).
  info("Test _generated_background_page.html set as subframe url");
  const extpage = await ExtensionTestUtils.loadContentPage(
    extUnpacked.extension.baseURI.resolve("extpage.html")
  );
  const result = await extpage.spawn([], () => {
    return new Promise(resolve => {
      const doc = this.content.document;
      const frame = doc.createElement("iframe");
      frame.src = "_generated_background_page.html";
      frame.onload = () =>
        resolve({
          type: "load",
          url: frame.contentWindow.location.href,
        });
      frame.onerror = () =>
        resolve({
          type: "error",
          url: frame.contentWindow.location.href,
        });
      doc.body.append(frame);
    });
  });
  await extpage.close();

  Assert.deepEqual(
    result,
    {
      type: "load",
      url: `moz-extension://${extUnpacked.uuid}/_generated_background_page.html`,
    },
    "Expect _generated_background_page.html to be loaded successfully as a subframe"
  );

  Assert.deepEqual(
    await extUnpacked.awaitMessage("frame-load-result"),
    {
      type: "load",
      url: `moz-extension://${extUnpacked.uuid}/iframe.html`,
    },
    "Expect moz-extension subframe to be loaded successfully in _generated_background_page subframe"
  );

  await extUnpacked.unload();
});
