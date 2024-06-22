/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

registerCleanupFunction(() =>
  Services.prefs.clearUserPref("browser.ml.chat.provider")
);

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
