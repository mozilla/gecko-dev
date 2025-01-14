/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the following aspects of the `quick-suggest` ping:
//
// * `requestId` should be non-null in the ping when the ping is related to a
//   suggestion served from Merino
// * `undefined` and empty-string values should be recorded in the ping as
//   `null`

"use strict";

const MERINO_RESULT = {
  // undefined
  impression_url: undefined,
  // empty string
  advertiser: "",

  block_id: 1,
  url: "https://example.com/sponsored",
  title: "Sponsored suggestion",
  keywords: ["sponsored"],
  click_url: "https://example.com/click",
  iab_category: "22 - Shopping",
  provider: "adm",
  is_sponsored: true,
};

const index = 1;
const position = index + 1;

// Trying to avoid timeouts in TV mode.
requestLongerTimeout(3);

add_setup(async function () {
  await setUpTelemetryTest({
    merinoSuggestions: [MERINO_RESULT],
  });
});

add_task(async function () {
  let matchType = "firefox-suggest";
  let source = "merino";

  await doTelemetryTest({
    index,
    suggestion: MERINO_RESULT,
    // impression-only
    impressionOnly: {
      ping: {
        pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
        matchType,
        advertiser: MERINO_RESULT.advertiser,
        blockId: MERINO_RESULT.block_id.toString(),
        improveSuggestExperience: true,
        position,
        suggestedIndex: "-1",
        suggestedIndexRelativeToGroup: true,
        requestId: MerinoTestUtils.server.response.body.request_id,
        source,
        contextId: "",
        isClicked: false,
        reportingUrl: MERINO_RESULT.impression_url,
      },
    },
    // click
    click: {
      pings: [
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          matchType,
          advertiser: MERINO_RESULT.advertiser,
          blockId: MERINO_RESULT.block_id.toString(),
          improveSuggestExperience: true,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: MerinoTestUtils.server.response.body.request_id,
          source,
          contextId: "",
          isClicked: true,
          reportingUrl: MERINO_RESULT.impression_url,
        },
        {
          pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          matchType,
          advertiser: MERINO_RESULT.advertiser,
          blockId: MERINO_RESULT.block_id.toString(),
          improveSuggestExperience: true,
          position,
          suggestedIndex: "-1",
          suggestedIndexRelativeToGroup: true,
          requestId: MerinoTestUtils.server.response.body.request_id,
          source,
          contextId: "",
          reportingUrl: MERINO_RESULT.click_url,
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
            advertiser: MERINO_RESULT.advertiser,
            blockId: MERINO_RESULT.block_id.toString(),
            improveSuggestExperience: true,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: MerinoTestUtils.server.response.body.request_id,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: MERINO_RESULT.impression_url,
          },
          {
            pingType: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            matchType,
            advertiser: MERINO_RESULT.advertiser,
            blockId: MERINO_RESULT.block_id.toString(),
            improveSuggestExperience: true,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: MerinoTestUtils.server.response.body.request_id,
            source,
            contextId: "",
            iabCategory: MERINO_RESULT.iab_category,
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
            advertiser: MERINO_RESULT.advertiser,
            blockId: MERINO_RESULT.block_id.toString(),
            improveSuggestExperience: true,
            position,
            suggestedIndex: "-1",
            suggestedIndexRelativeToGroup: true,
            requestId: MerinoTestUtils.server.response.body.request_id,
            source,
            contextId: "",
            isClicked: false,
            reportingUrl: MERINO_RESULT.impression_url,
          },
        ],
      },
    ],
  });
});
