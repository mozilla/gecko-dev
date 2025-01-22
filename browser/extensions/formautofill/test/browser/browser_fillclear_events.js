/* global add_autofill_heuristic_tests */

"use strict";

// This test verifies that the correct events get fired during the
// autofill and clear actions.

const TEST_PROFILE = {
  "given-name": "Jim",
  "family-name": "Walker",
  "street-address": "160 Main St",
  "address-level2": "Somewhere",
};

let expectedEvents = [
  // initialization during add_autofill_heuristic_tests will focus the last field
  "blur:city",

  // events during autofill
  "focus:given-name",
  "input:given-name",
  "change:given-name",
  "blur:given-name",
  "focus:family-name",
  "input:family-name",
  "change:family-name",
  "blur:family-name",
  "focus:street-address",
  "input:street-address",
  "change:street-address",
  "blur:street-address",
  "focus:city",
  "input:city",
  "change:city",

  // events during clear
  "blur:city",
  "focus:given-name",
  "input:given-name",
  "change:given-name",
  "blur:given-name",
  "focus:family-name",
  "input:family-name",
  "change:family-name",
  "blur:family-name",
  "focus:street-address",
  "input:street-address",
  "change:street-address",
  "blur:street-address",
  "focus:city",
  "input:city",
  "change:city",
  "blur:city",
  // after clearing all fields, the focused field is focused again.
  "focus:given-name",
];

add_autofill_heuristic_tests(
  [
    {
      fixtureData: `<form>
                     <input id="given-name" autocomplete="given-name">
                     <input id="additional-name" autocomplete="additional-name">
                     <input id="family-name" autocomplete="family-name">
                     <input id="street-address" autocomplete="street-address">
                     <input id="city" autocomplete="address-level2">
                  </form>`,
      profile: TEST_PROFILE,
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
            {
              fieldName: "additional-name",
              autofill: TEST_PROFILE["additional-name"],
            },
            { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
            {
              fieldName: "street-address",
              autofill: TEST_PROFILE["street-address"],
            },
            {
              fieldName: "address-level2",
              autofill: TEST_PROFILE["address-level2"],
            },
          ],
        },
      ],
      onTestStart: async () => {
        await SpecialPowers.spawn(
          gBrowser.selectedBrowser.browsingContext,
          [],
          () => {
            content.events = [];

            function logEvent(event) {
              if (event.target != content && event.target != content.document) {
                content.events.push(event.type + ":" + event.target.id);
              }
            }

            content.addEventListener("input", logEvent, true);
            content.addEventListener("change", logEvent, true);
            content.addEventListener("focus", logEvent, true);
            content.addEventListener("blur", logEvent, true);
          }
        );
      },
      onTestComplete: async () => {
        let events = await SpecialPowers.spawn(
          gBrowser.selectedBrowser.browsingContext,
          [],
          () => {
            return content.events;
          }
        );

        SimpleTest.isDeeply(events, expectedEvents, "expected events");
      },
    },
  ],
  "fixtures/"
);
