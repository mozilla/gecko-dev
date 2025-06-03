/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  NewTabContentPing: "resource://newtab/lib/NewTabContentPing.sys.mjs",
});

const MAX_SUBMISSION_DELAY = Services.prefs.getIntPref(
  "browser.newtabpage.activity-stream.telemetry.privatePing.maxSubmissionDelayMs",
  5000
);

add_setup(() => {
  do_get_profile();
  Services.fog.initializeFOG();
});

/**
 * Tests that the recordEvent method will cause a delayed ping submission to be
 * scheduled, and that the right extra fields are stripped from events.
 */
add_task(async function test_recordEvent_sanitizes_and_buffers() {
  let ping = new NewTabContentPing();

  // These fields are expected to be stripped before they get recorded in the
  // event.
  let sanitizedFields = {
    newtab_visit_id: "some visit id",
    tile_id: "some tile id",
    matches_selected_topic: "not-set",
    recommended_at: "1748877997039",
    received_rank: 0,
    event_source: "card",
  };

  // These fields are expected to survive the sanitization.
  let expectedFields = {
    section: "business",
    section_position: "2",
    position: "12",
    selected_topics: "",
    corpus_item_id: "7fc404a1-74ec-450b-8eef-4f52b45ec510",
    topic: "business",
    format: "medium-card",
    scheduled_corpus_item_id: "40f9ba69-1288-4778-8cfa-937df633819c",
    is_sponsored: "false",
    is_section_followed: "false",
  };

  ping.recordEvent("click", {
    // These should be sanitized out.
    ...sanitizedFields,

    ...expectedFields,
  });

  let extraMetrics = {
    utcOffset: "1",
    experimentBranch: "some-branch",
  };
  ping.scheduleSubmission(extraMetrics);

  await GleanPings.newtabContent.testSubmission(
    () => {
      let [clickEvent] = Glean.newtabContent.click.testGetValue();
      Assert.ok(clickEvent, "Found click event.");
      for (let fieldName of Object.keys(sanitizedFields)) {
        Assert.equal(
          clickEvent.extra[fieldName],
          undefined,
          `Should not have gotten sanitized extra field: ${fieldName}`
        );
      }
      for (let fieldName of Object.keys(expectedFields)) {
        Assert.equal(
          clickEvent.extra[fieldName],
          expectedFields[fieldName],
          `Should have recorded expected extra field: ${fieldName}`
        );
      }

      for (let metricName of Object.keys(extraMetrics)) {
        Assert.equal(
          Glean.newtabContent[metricName].testGetValue(),
          extraMetrics[metricName],
          `Should have recorded metric: ${metricName}`
        );
      }
    },
    () => {
      let delay = ping.testOnlyForceFlush();
      Assert.greater(delay, 1000, "Picked a random value greater than 1000");
      Assert.less(
        delay,
        MAX_SUBMISSION_DELAY,
        "Picked a random value less than MAX_SUBMISSION_DELAY"
      );
    }
  );
});
