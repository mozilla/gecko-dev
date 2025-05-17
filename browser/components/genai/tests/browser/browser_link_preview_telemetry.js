/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    clear: [["browser.ml.linkPreview.optin"]],
  });
});

/**
 * Test that AI consent telemetry records the "continue" (accept) option
 */
add_task(async function test_link_preview_ai_consent_continue() {
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.optin", true]],
  });

  let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 1, "One consent event recorded");
  Assert.equal(events[0].extra.option, "continue", "Continue option recorded");
});

/**
 * Test that AI consent telemetry records the "cancel" (deny) option
 */
add_task(async function test_link_preview_ai_consent_cancel() {
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.linkPreview.optin", false]],
  });

  let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 1, "One consent event recorded");
  Assert.equal(events[0].extra.option, "cancel", "Cancel option recorded");
});

/**
 * Test that AI consent telemetry records the "learn" option
 */
add_task(async function test_link_preview_ai_consent_learn() {
  Services.fog.testResetFOG();

  Glean.genaiLinkpreview.cardAiConsent.record({ option: "learn" });

  let events = Glean.genaiLinkpreview.cardAiConsent.testGetValue();
  Assert.equal(events.length, 1, "One consent event recorded");
  Assert.equal(events[0].extra.option, "learn", "Learn option recorded");
});
