/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for sponsored suggestions.
 */

"use strict";

const REMOTE_SETTINGS_RESULT = {
  id: 1,
  url: "https://example.com/sponsored",
  title: "Sponsored suggestion",
  keywords: ["sponsored"],
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "testadvertiser",
  iab_category: "22 - Shopping",
  icon: "1234",
};

const suggestion_type = "sponsored";
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

// sponsored
add_task(async function sponsored() {
  let match_type = "firefox-suggest";
  let source = UrlbarPrefs.get("quicksuggest.rustEnabled")
    ? "rust"
    : "remote-settings";

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
            suggested_index: -1,
            suggested_index_relative_to_group: true,
            improve_suggest_experience_checked,
            is_clicked: false,
            block_id: REMOTE_SETTINGS_RESULT.id,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              is_clicked: true,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            payload: {
              source,
              match_type,
              position,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              payload: {
                source,
                match_type,
                position,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              },
            },
          ],
        },
      ],
    });
    await SpecialPowers.popPrefEnv();
  }
});

// higher-placement sponsored, a.k.a sponsored priority, sponsored best match
add_task(async function sponsoredBestMatch() {
  let match_type = "best-match";
  let source = UrlbarPrefs.get("quicksuggest.rustEnabled")
    ? "rust"
    : "remote-settings";

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quicksuggest.sponsoredPriority", true]],
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
          suggested_index: 1,
          suggested_index_relative_to_group: false,
          is_clicked: false,
          improve_suggest_experience_checked: false,
          block_id: REMOTE_SETTINGS_RESULT.id,
          advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
            suggested_index: 1,
            suggested_index_relative_to_group: false,
            is_clicked: true,
            improve_suggest_experience_checked: false,
            block_id: REMOTE_SETTINGS_RESULT.id,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
          },
        },
        {
          type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          payload: {
            source,
            match_type,
            position,
            suggested_index: 1,
            suggested_index_relative_to_group: false,
            improve_suggest_experience_checked: false,
            block_id: REMOTE_SETTINGS_RESULT.id,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            payload: {
              source,
              match_type,
              position,
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
        ],
      },
    ],
  });
  await SpecialPowers.popPrefEnv();
});
