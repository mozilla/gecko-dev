/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for nonsponsored suggestions.
 */

"use strict";

const REMOTE_SETTINGS_RESULT = {
  id: 1,
  url: "https://example.com/nonsponsored",
  title: "Non-sponsored suggestion",
  keywords: ["nonsponsored"],
  advertiser: "Wikipedia",
  iab_category: "5 - Education",
  icon: "1234",
};

const suggestion_type = "nonsponsored";
const index = 1;
const position = index + 1;

// Trying to avoid timeouts in TV mode.
requestLongerTimeout(3);

add_setup(async function () {
  await setUpTelemetryTest({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: [REMOTE_SETTINGS_RESULT],
      },
    ],
  });
});

add_task(async function nonsponsored() {
  let match_type = "firefox-suggest";
  let advertiser = REMOTE_SETTINGS_RESULT.advertiser.toLowerCase();
  let reporting_url = undefined;
  let source = "rust";

  // Make sure `improve_suggest_experience_checked` is recorded correctly
  // depending on the value of the related pref.
  for (let improve_suggest_experience_checked of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.urlbar.quicksuggest.dataCollection.enabled",
          improve_suggest_experience_checked,
        ],
      ],
    });
    await doTelemetryTest({
      index,
      suggestion: REMOTE_SETTINGS_RESULT,
      // impression-only
      impressionOnly: {
        ping: {
          type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          payload: {
            source,
            match_type,
            position,
            advertiser,
            reporting_url,
            suggested_index: -1,
            suggested_index_relative_to_group: true,
            improve_suggest_experience_checked,
            is_clicked: false,
          },
        },
      },
      // click
      click: {
        pings: [
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            payload: {
              source,
              match_type,
              position,
              advertiser,
              reporting_url,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              is_clicked: true,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            payload: {
              source,
              match_type,
              position,
              advertiser,
              reporting_url,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
            },
          },
        ],
      },
      commands: [
        // dismiss
        {
          command: "dismiss",
          pings: [
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              payload: {
                source,
                match_type,
                position,
                advertiser,
                reporting_url,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              payload: {
                source,
                match_type,
                position,
                advertiser,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                iab_category: REMOTE_SETTINGS_RESULT.iab_category,
              },
            },
          ],
        },
        // manage
        {
          command: "manage",
          pings: [
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              payload: {
                source,
                match_type,
                position,
                advertiser,
                reporting_url,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
              },
            },
          ],
        },
      ],
    });
    await SpecialPowers.popPrefEnv();
  }
});
