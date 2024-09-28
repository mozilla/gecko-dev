/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { GenAI } = ChromeUtils.importESModule(
  "resource:///modules/GenAI.sys.mjs"
);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.ml.chat.enabled");
  Services.prefs.clearUserPref("browser.ml.chat.provider");
  Services.prefs.clearUserPref("sidebar.main.tools");
  Services.prefs.clearUserPref("sidebar.revamp");
});

/**
 * Check various prefs for showing chat
 */
add_task(async function test_show_chat() {
  Assert.ok(!GenAI.canShowChatEntrypoint, "Default no");

  Services.prefs.setBoolPref("browser.ml.chat.enabled", true);

  Assert.ok(!GenAI.canShowChatEntrypoint, "Not enough to just enable");

  Services.prefs.setStringPref(
    "browser.ml.chat.provider",
    "http://mochi.test:8888"
  );

  Assert.ok(GenAI.canShowChatEntrypoint, "Can show with provider");

  Services.prefs.setBoolPref("sidebar.revamp", true);

  Assert.ok(GenAI.canShowChatEntrypoint, "Can show with revamp");

  Services.prefs.setStringPref("sidebar.main.tools", "history");

  Assert.ok(!GenAI.canShowChatEntrypoint, "Not shown without chatbot tool");

  Services.prefs.setBoolPref("sidebar.revamp", false);

  Assert.ok(GenAI.canShowChatEntrypoint, "Ignore tools without revamp");
});
