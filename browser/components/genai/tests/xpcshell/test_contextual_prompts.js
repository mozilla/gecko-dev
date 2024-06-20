/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

registerCleanupFunction(() =>
  Services.prefs.clearUserPref("browser.ml.chat.prompts.test")
);

/**
 * Check prompts from prefs are used
 */
add_task(async function test_prompt_prefs() {
  const origPrompts = (await GenAI.getContextualPrompts()).length;

  Services.prefs.setStringPref("browser.ml.chat.prompts.test", "{}");
  Assert.equal(
    (await GenAI.getContextualPrompts()).length,
    origPrompts + 1,
    "Added a prompt"
  );

  Services.prefs.setStringPref(
    "browser.ml.chat.prompts.test",
    JSON.stringify({ targeting: "false" })
  );
  Assert.equal(
    (await GenAI.getContextualPrompts()).length,
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
  const origPrompts = (await GenAI.getContextualPrompts()).length;

  Assert.equal(
    (await GenAI.getContextualPrompts({ provider: "localhost" })).length,
    origPrompts + 1,
    "Added contextual prompt"
  );

  Assert.equal(
    (await GenAI.getContextualPrompts({ provider: "" })).length,
    origPrompts,
    "Prompt not added for contextual targeting"
  );
});
