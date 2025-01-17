/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the `quick-suggest` ping, which is used for for AMP suggestions.

"use strict";

const SUGGESTION = QuickSuggestTestUtils.ampRemoteSettings();

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

add_task(async function amp() {
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
        blockId: SUGGESTION.id.toString(),
        improveSuggestExperience,
        position,
        suggestedIndex: "-1",
        suggestedIndexRelativeToGroup: true,
        requestId: undefined,
        source,
        contextId: "",
        isClicked: false,
        reportingUrl: SUGGESTION.impression_url,
      },
      click: [
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          matchType,
          advertiser,
          blockId: SUGGESTION.id.toString(),
          improveSuggestExperience,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: undefined,
          source,
          contextId: "",
          isClicked: true,
          reportingUrl: SUGGESTION.impression_url,
        },
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          matchType,
          advertiser,
          blockId: SUGGESTION.id.toString(),
          improveSuggestExperience,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: undefined,
          source,
          contextId: "",
          reportingUrl: SUGGESTION.click_url,
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
              blockId: SUGGESTION.id.toString(),
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: SUGGESTION.impression_url,
            },
            {
              pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              matchType,
              advertiser,
              blockId: SUGGESTION.id.toString(),
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
              blockId: SUGGESTION.id.toString(),
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: SUGGESTION.impression_url,
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
  let matchType = "best-match";
  let advertiser = SUGGESTION.advertiser.toLowerCase();
  let source = "rust";

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quicksuggest.sponsoredPriority", true]],
  });
  await doQuickSuggestPingTest({
    index,
    suggestion: SUGGESTION,
    impressionOnly: {
      pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
      matchType,
      advertiser,
      blockId: SUGGESTION.id.toString(),
      improveSuggestExperience: false,
      position,
      suggestedIndex: "1",
      suggestedIndexRelativeToGroup: false,
      requestId: undefined,
      source,
      contextId: "",
      isClicked: false,
      reportingUrl: SUGGESTION.impression_url,
    },
    click: [
      {
        pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
        matchType,
        advertiser,
        blockId: SUGGESTION.id.toString(),
        improveSuggestExperience: false,
        position,
        suggestedIndex: "1",
        suggestedIndexRelativeToGroup: false,
        requestId: undefined,
        source,
        contextId: "",
        isClicked: true,
        reportingUrl: SUGGESTION.impression_url,
      },
      {
        pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
        matchType,
        advertiser,
        blockId: SUGGESTION.id.toString(),
        improveSuggestExperience: false,
        position,
        suggestedIndex: "1",
        suggestedIndexRelativeToGroup: false,
        requestId: undefined,
        source,
        contextId: "",
        reportingUrl: SUGGESTION.click_url,
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
            blockId: SUGGESTION.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: SUGGESTION.impression_url,
          },
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            matchType,
            advertiser,
            blockId: SUGGESTION.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
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
            blockId: SUGGESTION.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: SUGGESTION.impression_url,
          },
        ],
      },
    ],
  });
  await SpecialPowers.popPrefEnv();
});
