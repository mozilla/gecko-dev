/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AboutWelcomeTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs"
);
const TELEMETRY_PREF = "browser.newtabpage.activity-stream.telemetry";

add_setup(function setup() {
  do_get_profile();
  Services.prefs.setBoolPref(TELEMETRY_PREF, true);
  Services.fog.initializeFOG();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(TELEMETRY_PREF);
  });
});

// We recognize two kinds of unexpected data that might reach
// `submitGleanPingForPing`: unknown keys, and keys with unexpectedly-complex
// data (ie, non-scalar).
// We report the keys in special metrics to aid in system health monitoring.
add_task(async function test_weird_data() {
  const AWTelemetry = new AboutWelcomeTelemetry();

  const unknownKey = "some_unknown_key";
  const camelUnknownKey = AWTelemetry._snakeToCamelCase(unknownKey);

  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.unknownKeys[camelUnknownKey].testGetValue(),
        1,
        "caught the unknown key"
      );
      // TODO(bug 1600008): Also check the for-testing overall count.
      Assert.equal(Glean.messagingSystem.unknownKeyCount.testGetValue(), 1);
    },
    () =>
      AWTelemetry.submitGleanPingForPing({
        [unknownKey]: "value doesn't matter",
      })
  );

  const invalidNestedDataKey = "event";
  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.invalidNestedData[
          invalidNestedDataKey
        ].testGetValue("messaging-system"),
        1,
        "caught the invalid nested data"
      );
    },
    () =>
      AWTelemetry.submitGleanPingForPing({
        [invalidNestedDataKey]: { this_should: "not be", complex: "data" },
      })
  );
});

// `event_context` is weird. It's an object, but it might have been stringified
// before being provided for recording.
add_task(async function test_event_context() {
  const AWTelemetry = new AboutWelcomeTelemetry();

  const eventContext = {
    reason: "reason",
    page: "page",
    source: "source",
    something_else: "not specifically handled",
    screen_family: "family",
    screen_id: "screen_id",
    screen_index: 0,
    screen_initlals: "screen_initials",
  };
  const stringifiedEC = JSON.stringify(eventContext);

  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.eventReason.testGetValue(),
        eventContext.reason,
        "event_context.reason also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventPage.testGetValue(),
        eventContext.page,
        "event_context.page also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventSource.testGetValue(),
        eventContext.source,
        "event_context.source also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenFamily.testGetValue(),
        eventContext.screen_family,
        "event_context.screen_family also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenId.testGetValue(),
        eventContext.screen_id,
        "event_context.screen_id also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenIndex.testGetValue(),
        eventContext.screen_index,
        "event_context.screen_index also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenInitials.testGetValue(),
        eventContext.screen_initials,
        "event_context.screen_initials also in own metric."
      );

      Assert.equal(
        Glean.messagingSystem.eventContext.testGetValue(),
        stringifiedEC,
        "whole event_context added as text."
      );
    },
    () =>
      AWTelemetry.submitGleanPingForPing({
        event_context: eventContext,
      })
  );

  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.eventReason.testGetValue(),
        eventContext.reason,
        "event_context.reason also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventPage.testGetValue(),
        eventContext.page,
        "event_context.page also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventSource.testGetValue(),
        eventContext.source,
        "event_context.source also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenFamily.testGetValue(),
        eventContext.screen_family,
        "event_context.screen_family also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenId.testGetValue(),
        eventContext.screen_id,
        "event_context.screen_id also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenIndex.testGetValue(),
        eventContext.screen_index,
        "event_context.screen_index also in own metric."
      );
      Assert.equal(
        Glean.messagingSystem.eventScreenInitials.testGetValue(),
        eventContext.screen_initials,
        "event_context.screen_initials also in own metric."
      );

      Assert.equal(
        Glean.messagingSystem.eventContext.testGetValue(),
        stringifiedEC,
        "whole event_context added as text."
      );
    },
    () =>
      AWTelemetry.submitGleanPingForPing({
        event_context: stringifiedEC,
      })
  );
});

// For event_context to be more useful, we want to make sure we don't error
// in cases where it doesn't make much sense, such as a plain string that
// doesnt attempt to represent a valid object.
add_task(async function test_context_errors() {
  const AWTelemetry = new AboutWelcomeTelemetry();

  let weird_context_ping = {
    event_context: "oops, this string isn't a valid JS object!",
  };

  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.eventContextParseError.testGetValue(),
        undefined,
        "this poorly formed context shouldn't register because it was not an object!"
      );
    },
    () => AWTelemetry.submitGleanPingForPing(weird_context_ping)
  );

  weird_context_ping = {
    event_context:
      "{oops : {'this string isn't a valid JS object, but it sure looks like one!}}'",
  };

  await GleanPings.messagingSystem.testSubmission(
    () => {
      Assert.equal(
        Glean.messagingSystem.eventContextParseError.testGetValue(),
        1,
        "this poorly formed context should register because it was not an object!"
      );
    },
    () => AWTelemetry.submitGleanPingForPing(weird_context_ping)
  );
});
