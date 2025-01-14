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
  let matchType = "firefox-suggest";
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
          advertiser: REMOTE_SETTINGS_RESULT.advertiser,
          blockId: REMOTE_SETTINGS_RESULT.id.toString(),
          improveSuggestExperience,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: undefined,
          source,
          contextId: "",
          isClicked: false,
          reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
        },
      },
      // click
      click: {
        pings: [
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            matchType,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            blockId: REMOTE_SETTINGS_RESULT.id.toString(),
            improveSuggestExperience,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: true,
            reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
          },
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            matchType,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            blockId: REMOTE_SETTINGS_RESULT.id.toString(),
            improveSuggestExperience,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: undefined,
            source,
            contextId: "",
            reportingUrl: REMOTE_SETTINGS_RESULT.click_url,
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
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              blockId: REMOTE_SETTINGS_RESULT.id.toString(),
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
            },
            {
              pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              matchType,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              blockId: REMOTE_SETTINGS_RESULT.id.toString(),
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
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              blockId: REMOTE_SETTINGS_RESULT.id.toString(),
              improveSuggestExperience,
              position,
              suggestedIndex: "-1",
              suggestedIndexRelativeToGroup: true,
              requestId: undefined,
              source,
              contextId: "",
              isClicked: false,
              reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
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
  let source = "rust";

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quicksuggest.sponsoredPriority", true]],
  });
  await doTelemetryTest({
    index,
    suggestion: REMOTE_SETTINGS_RESULT,
    // impression-only
    impressionOnly: {
      ping: {
        pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
        matchType,
        advertiser: REMOTE_SETTINGS_RESULT.advertiser,
        blockId: REMOTE_SETTINGS_RESULT.id.toString(),
        improveSuggestExperience: false,
        position,
        suggestedIndex: "1",
        suggestedIndexRelativeToGroup: false,
        requestId: undefined,
        source,
        contextId: "",
        isClicked: false,
        reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
      },
    },
    // click
    click: {
      pings: [
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          matchType,
          advertiser: REMOTE_SETTINGS_RESULT.advertiser,
          blockId: REMOTE_SETTINGS_RESULT.id.toString(),
          improveSuggestExperience: false,
          position,
          suggestedIndex: "1",
          suggestedIndexRelativeToGroup: false,
          requestId: undefined,
          source,
          contextId: "",
          isClicked: true,
          reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
        },
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          matchType,
          advertiser: REMOTE_SETTINGS_RESULT.advertiser,
          blockId: REMOTE_SETTINGS_RESULT.id.toString(),
          improveSuggestExperience: false,
          position,
          suggestedIndex: "1",
          suggestedIndexRelativeToGroup: false,
          requestId: undefined,
          source,
          contextId: "",
          reportingUrl: REMOTE_SETTINGS_RESULT.click_url,
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
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            blockId: REMOTE_SETTINGS_RESULT.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
          },
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            matchType,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            blockId: REMOTE_SETTINGS_RESULT.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
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
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            blockId: REMOTE_SETTINGS_RESULT.id.toString(),
            improveSuggestExperience: false,
            position,
            suggestedIndex: "1",
            suggestedIndexRelativeToGroup: false,
            requestId: undefined,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: REMOTE_SETTINGS_RESULT.impression_url,
          },
        ],
      },
    ],
  });
  await SpecialPowers.popPrefEnv();
});
