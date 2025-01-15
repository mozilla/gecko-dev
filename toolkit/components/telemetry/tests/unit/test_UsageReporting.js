/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await TelemetryController.testSetup();

  // We need to initialize FOG once, otherwise operations will be stuck in the
  // pre-init queue.
  Services.fog.initializeFOG();
});

// Verify that rising and falling edges are handled as expected.
add_task(async function test_prefs() {
  // The default value is `true`, so this should be a no-op.  But belt and braces...
  Services.prefs.setBoolPref("datareporting.usage.uploadEnabled", true);

  // In Firefox itself, these are set as part of browser startup.  In
  // tests, we need to arrange our initial state.
  GleanPings.usageReporting.setEnabled(true);
  GleanPings.usageDeletionRequest.setEnabled(true);

  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.usage.profileId.set(kTestUuid);
  Assert.equal(
    kTestUuid,
    Glean.usage.profileId.testGetValue("usage-reporting")
  );
  Assert.equal(
    kTestUuid,
    Glean.usage.profileId.testGetValue("usage-deletion-request")
  );

  let deletionRequestSubmitted = false;
  GleanPings.usageDeletionRequest.testBeforeNextSubmit(reason => {
    deletionRequestSubmitted = true;

    // The deletion request needs the previous identifier.
    Assert.equal(
      kTestUuid,
      Glean.usage.profileId.testGetValue("usage-deletion-request")
    );

    Assert.equal(reason, "set_upload_enabled");
  });

  // Disable the pref: we should witness a falling edge.
  Services.prefs.setBoolPref("datareporting.usage.uploadEnabled", false);

  Assert.ok(
    deletionRequestSubmitted,
    "'usage-deletion-request' ping submitted"
  );

  // We can't inspect the enablement of a ping directly (yet), so we witness
  // related behaviour instead.  Disabling a ping clears the stored values for
  // that ping.
  Assert.equal(null, Glean.usage.profileId.testGetValue("usage-reporting"));

  // Enable the pref: we should witness a rising edge.
  Services.prefs.setBoolPref("datareporting.usage.uploadEnabled", true);

  let usageReportingSubmitted = false;
  GleanPings.usageReporting.testBeforeNextSubmit(_reason => {
    usageReportingSubmitted = true;

    // The usage reporting ping needs an identifier.
    Assert.notEqual(
      null,
      Glean.usage.profileId.testGetValue("usage-reporting")
    );

    // And it should *not* be the old identifier; toggling the preference should
    // abandon any existing usage reporting data.
    Assert.notEqual(
      kTestUuid,
      Glean.usage.profileId.testGetValue("usage-reporting")
    );
  });

  // In Firefox itself, the usage reporting ping is sent on the baseline ping
  // schedule.  In testing, we have to do it by hand.
  GleanPings.usageReporting.submit();

  Assert.ok(usageReportingSubmitted, "'usage-reporting' ping submitted");
});
