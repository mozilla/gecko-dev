/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

registerCleanupFunction(() =>
  Services.prefs.clearUserPref("browser.ml.chat.prompts.test")
);

const DEFAULT_CONTEXT = { provider: "" };
async function promptCount(context = DEFAULT_CONTEXT) {
  return (await GenAI.getContextualPrompts(context)).length;
}

/**
 * Check prompts from prefs are used
 */
add_task(async function test_prompt_prefs() {
  const origPrompts = await promptCount();

  Services.prefs.setStringPref("browser.ml.chat.prompts.test", "{}");
  Assert.equal(await promptCount(), origPrompts + 1, "Added a prompt");

  Services.prefs.setStringPref(
    "browser.ml.chat.prompts.test",
    JSON.stringify({ targeting: "false" })
  );
  Assert.equal(
    await promptCount(),
    origPrompts,
    "Prompt not added for targeting"
  );
});

/**
 * Check context is used for targeting
 */
add_task(async function test_prompt_context() {
  Services.prefs.setStringPref(
    "browser.ml.chat.prompts.test",
    JSON.stringify({ targeting: "provider" })
  );
  const origPrompts = await promptCount();

  Assert.equal(
    await promptCount({ provider: "localhost" }),
    origPrompts + 1,
    "Added contextual prompt"
  );

  Assert.equal(
    await promptCount(),
    origPrompts,
    "Prompt not added for contextual targeting"
  );
});

/**
 * Check that prompts can be localized
 */
add_task(async function test_prompt_l10n() {
  Services.prefs.setStringPref(
    "browser.ml.chat.prompts.test",
    JSON.stringify({
      l10nId: "genai-prompts-summarize",
      label: "bad",
      test: true,
    })
  );
  const custom = (await GenAI.getContextualPrompts(DEFAULT_CONTEXT)).find(
    prompt => prompt.id == null
  );

  Assert.ok(custom.test, "Found the custom prompt");
  Assert.notEqual(custom.label, "bad", "Localized label takes priority");
  Assert.ok(custom.value, "Got localized prompt");
});
