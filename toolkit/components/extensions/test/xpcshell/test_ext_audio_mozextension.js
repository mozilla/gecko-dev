"use strict";

const { ExtensionTestCommon } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

AddonTestUtils.init(this);

add_setup(async () => {
  await ExtensionTestUtils.startAddonManager();
});

// Verifies that audio can successfully be loaded. For details on reproduction
// and logs, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1727383#c19
add_task(async function test_audio_from_unpacked_moz_extension() {
  let files = ExtensionTestCommon.generateFiles({
    files: {
      "test.mp3": (await IOUtils.read(do_get_file("data/test.mp3").path))
        .buffer,
    },
    manifest: {
      browser_specific_settings: { gecko: { id: "test@ext" } },
    },
    async background() {
      await new Promise(resolve => {
        const mozExtensionUrl = browser.runtime.getURL("test.mp3");
        browser.test.log(`Loading Audio: ${mozExtensionUrl}`);
        let audio = new Audio(mozExtensionUrl);
        audio.onloadedmetadata = resolve;
      });
      browser.test.sendMessage("done");
    },
  });

  const unpackedPath = await IOUtils.createUniqueDirectory(
    AddonTestUtils.tempDir.path,
    "unpacked-extension"
  );

  const dir = await AddonTestUtils.promiseWriteFilesToDir(unpackedPath, files);
  const extension = ExtensionTestUtils.expectExtension("test@ext");
  await AddonManager.installTemporaryAddon(dir);

  await extension.awaitStartup();
  await extension.awaitMessage("done");
  await extension.unload();

  await IOUtils.remove(unpackedPath, { recursive: true });
});
