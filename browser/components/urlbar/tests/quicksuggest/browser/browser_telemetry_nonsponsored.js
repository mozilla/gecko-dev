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
  let matchType = "firefox-suggest";
  let advertiser = REMOTE_SETTINGS_RESULT.advertiser.toLowerCase();
  let source = "rust";

  // Make sure `improveSuggestExperience` is recorded correctly depending on the
  // value of the related pref.
  for (let improveSuggestExperience of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.urlbar.quicksuggest.dataCollection.enabled",
          improveSuggestExperience,
        ],
      ],
    });
    await doTelemetryTest({
      index,
      suggestion: REMOTE_SETTINGS_RESULT,
      // impression-only
      impressionOnly: {
        ping: {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          matchType,
          advertiser,
          blockId: undefined,
          improveSuggestExperience,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: undefined,
          source,
          contextId: "",
          isClicked: false,
          reportingUrl: undefined,
        },
      },
      // click
      click: {
        pings: [
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            matchType,
            advertiser,
            blockId: undefined,
            improveSuggestExperience,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: true,
            reportingUrl: undefined,
          },
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            matchType,
            advertiser,
            blockId: undefined,
            improveSuggestExperience,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: undefined,
            source,
            contextId: "",
            reportingUrl: undefined,
          },
        ],
      },
      commands: [
        // dismiss
        {
          command: "dismiss",
          pings: [
            {
              pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              matchType,
              advertiser,
              blockId: undefined,
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: undefined,
            },
            {
              pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              matchType,
              advertiser,
              blockId: undefined,
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              iabCategory: REMOTE_SETTINGS_RESULT.iab_category,
            },
          ],
        },
        // manage
        {
          command: "manage",
          pings: [
            {
              pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              matchType,
              advertiser,
              blockId: undefined,
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: undefined,
            },
          ],
        },
      ],
    });
    await SpecialPowers.popPrefEnv();
  }
});
