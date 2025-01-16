/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the `quick-suggest` ping for Wikipedia (nonsponsored) suggestions.

"use strict";

const SUGGESTION = QuickSuggestTestUtils.wikipediaRemoteSettings();

const index = 1;
const position = index + 1;

// Trying to avoid timeouts in TV mode.
requestLongerTimeout(3);

add_setup(async function () {
  await initQuickSuggestPingTest({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: [SUGGESTION],
      },
    ],
  });
});

add_task(async function wikipedia() {
  let matchType = "firefox-suggest";
  let advertiser = SUGGESTION.advertiser.toLowerCase();
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
    await doQuickSuggestPingTest({
      index,
      suggestion: SUGGESTION,
      impressionOnly: {
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
      click: [
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
      commands: [
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
              iabCategory: SUGGESTION.iab_category,
            },
          ],
        },
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
