/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_SET_SEARCH_MODE() {
  const action = {
    type: "SET_SEARCH_MODE",
    data: {
      engineName: "Perplexity",
      isGeneralPurposeEngine: true,
      source: 3,
      isPreview: false,
      entry: "other",
    },
    dismiss: true,
  };
  await SMATestUtils.validateAction(action);

  // TODO: Add action execution test
  // Bug 1964110 - Add test for Special Action to set search mode
});
