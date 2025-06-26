/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

add_setup(() => {
  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "");
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("browser.ml.chat.prompt.prefix")
  );
});

/**
 * Check that prompts come from label or value
 */
add_task(function test_basic_prompt() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a" }),
    "a",
    "Uses label for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ value: "b" }),
    "b",
    "Uses value for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a", value: "b" }),
    "b",
    "Prefers value for prompt"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "a", value: "" }),
    "a",
    "Falls back to label for prompt"
  );
});

/**
 * Check that placeholders can use context
 */
add_task(function test_prompt_placeholders() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }),
    "<a>%a%</a>",
    "Placeholder kept without context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }, { a: "z" }),
    "<a>z</a>",
    "Placeholder replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%%a%%a%" }, { a: "z" }),
    "<a>z</a><a>z</a><a>z</a>",
    "Repeat placeholders replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "z" }),
    "<a>z</a> <b>%b%</b>",
    "Missing placeholder context not replaced"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "z", b: "y" }),
    "<a>z</a> <b>y</b>",
    "Multiple placeholders replaced with context"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a% %b%" }, { a: "%b%", b: "y" }),
    "<a>%b%</a> <b>y</b>",
    "Placeholders from original prompt replaced with context"
  );
});

/**
 * Check that placeholder options are used
 */
add_task(function test_prompt_placeholder_options() {
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|1%" }, { a: "xyz" }),
    "<a>x</a>",
    "Context reduced to 1"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|2%" }, { a: "xyz" }),
    "<a>xy</a>",
    "Context reduced to 2"
  );
  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a|3%" }, { a: "xyz" }),
    "<a>xyz</a>",
    "Context kept to 3"
  );
});

/**
 * Check that prefix pref is added to prompt
 */
add_task(async function test_prompt_prefix() {
  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "hello");
  await GenAI.prepareChatPromptPrefix();

  Assert.equal(
    GenAI.buildChatPrompt({ label: "world" }),
    "hello\n\nworld",
    "Prefix and prompt combined"
  );

  Services.prefs.setStringPref("browser.ml.chat.prompt.prefix", "%a%");
  await GenAI.prepareChatPromptPrefix();

  Assert.equal(
    GenAI.buildChatPrompt({ label: "%a%" }, { a: "hi" }),
    "<a>hi</a>\n\n<a>hi</a>",
    "Context used for prefix and prompt"
  );
});

/**
 * Check that prefix pref supports localization
 */
add_task(async function test_prompt_prefix() {
  Services.prefs.clearUserPref("browser.ml.chat.prompt.prefix");
  await GenAI.prepareChatPromptPrefix();

  Assert.ok(
    JSON.parse(Services.prefs.getStringPref("browser.ml.chat.prompt.prefix"))
      .l10nId,
    "Default prefix is localized"
  );

  Assert.ok(
    !GenAI.buildChatPrompt({ label: "" }).match(/l10nId/),
    "l10nId replaced with localized"
  );
});

/**
 * Check that selection limits are estimated
 */
add_task(async function test_estimate_limit() {
  const length = 1234;
  const limit = GenAI.estimateSelectionLimit(length);
  Assert.ok(limit, "Got some limit");
  Assert.ok(limit < length, "Limit smaller than length");

  const defaultLimit = GenAI.estimateSelectionLimit();
  Assert.ok(defaultLimit, "Got a default limit");
  Assert.ok(defaultLimit > limit, "Default uses a larger length");
});

/**
 * Check that prefix pref supports dynamic limit
 */
add_task(async function test_prompt_limit() {
  const getLength = () => GenAI.chatPromptPrefix.match(/selection\|(\d+)/)[1];
  await GenAI.prepareChatPromptPrefix();

  const length = getLength();
  Assert.ok(length, "Got a max length by default");

  Services.prefs.setStringPref(
    "browser.ml.chat.provider",
    "http://localhost:8080"
  );
  await GenAI.prepareChatPromptPrefix();

  const newLength = getLength();
  Assert.ok(newLength, "Got another max length");
  Assert.ok(newLength != length, "Lengths changed with provider change");

  Services.prefs.clearUserPref("browser.ml.chat.provider");
});
