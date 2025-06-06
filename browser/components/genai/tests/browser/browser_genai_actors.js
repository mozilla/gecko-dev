/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that actor gets added when enabled
 */
add_task(async function test_got_actor() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://localhost:8080"]],
  });
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    Assert.ok(
      browser.browsingContext.currentWindowContext.getActor("GenAI"),
      "Got the actor"
    );
  });
});

/**
 * Test the actor gets text from webpage
 */
add_task(async function test_actor_extract_text() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "data:text/html, <body>Hello world!</body>",
    },
    async browser => {
      const actor =
        browser.browsingContext.currentWindowContext.getActor("GenAI");
      Assert.ok(actor, "GenAI should be attached to this tab");

      const innerText = await actor.sendQuery("GetReadableText");
      Assert.ok(innerText.includes("Hello world!"), "Page text was extracted");
    }
  );
});

/**
 * Check that actor not found when disabled
 */
add_task(async function test_actor_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", ""]],
  });
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    Assert.throws(
      () => browser.browsingContext.currentWindowContext.getActor("GenAI"),
      /NotFoundError/,
      "Actor disabled"
    );
  });
});
