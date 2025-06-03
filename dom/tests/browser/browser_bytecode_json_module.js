"use strict";

const PAGE_URL =
  "https://example.com/browser/dom/tests/browser/page_bytecode_cache_json_module.html";

async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

add_task(async function () {
  // Eagerly generate bytecode cache.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.script_loader.bytecode_cache.enabled", true],
      ["dom.script_loader.bytecode_cache.strategy", -1],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE_URL,
      waitForLoad: true,
    },
    async () => {
      // TODO: Once the bytecode-encoding events are refactored,
      //       listen to the events (bug 1902951).
      await waitForIdle();
      ok(true, "No crash should happen");
    }
  );

  await SpecialPowers.popPrefEnv();
});
