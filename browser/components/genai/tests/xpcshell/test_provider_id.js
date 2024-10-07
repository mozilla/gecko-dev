/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.ml.chat.provider");
  Services.prefs.clearUserPref("browser.ml.chat.providers");
});

/**
 * Check various provider ids are converted
 */
add_task(async function test_provider_id() {
  Assert.equal(GenAI.getProviderId(""), "none", "Empty is none");
  Assert.equal(
    GenAI.getProviderId("http://mochi.test"),
    "custom",
    "Unknown is custom"
  );
  Assert.equal(
    GenAI.getProviderId("http://localhost:8080"),
    "localhost",
    "Known gets an id"
  );
  Assert.equal(GenAI.getProviderId(), "none", "Default to empty");

  Services.prefs.setStringPref(
    "browser.ml.chat.provider",
    "http://mochi.test:8888"
  );
  Assert.equal(GenAI.getProviderId(), "custom", "Used custom pref");
});

/**
 * Check that providers can be hidden
 */
add_task(async function test_hide_providers() {
  const chatgpt = GenAI.chatProviders.get("https://chatgpt.com");

  Assert.ok(!chatgpt.hidden, "ChatGPT shown by default");

  Services.prefs.setStringPref("browser.ml.chat.providers", "");

  Assert.ok(chatgpt.hidden, "ChatGPT hidden");
});

/**
 * Check that providers can be ordered
 */
add_task(async function test_providers_order() {
  Services.prefs.setStringPref(
    "browser.ml.chat.providers",
    "huggingchat,chatgpt"
  );

  const shown = [];
  GenAI.chatProviders.forEach(val => {
    if (!val.hidden) {
      shown.push(val.id);
    }
  });

  Assert.equal(shown, "huggingchat,chatgpt", "Providers reordered");
});
