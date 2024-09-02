/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertGleanTelemetry(events, expected_number_of_flowid = 1) {
  let flow_ids = new Set();
  events.forEach(({ event_name, expected_extra, event_count = 1 }) => {
    const actual_events = Glean.formautofill[event_name].testGetValue() ?? [];

    Assert.equal(
      actual_events.length,
      event_count,
      `Expected to have ${event_count} event/s with the name "${event_name}"`
    );

    expected_extra = Array.isArray(expected_extra)
      ? expected_extra
      : [expected_extra];
    for (let idx = 0; idx < actual_events.length; idx++) {
      const actual = actual_events[idx];
      const expected = expected_extra[idx];
      if (expected) {
        flow_ids.add(actual.extra.flow_id);
        delete actual.extra.flow_id; // We don't want to test the specific flow_id value yet

        Assert.deepEqual(actual.extra, expected);
      }
    }
  });

  Assert.equal(
    flow_ids.size,
    expected_number_of_flowid,
    `All events from the same user interaction session have the same flow id`
  );
}

add_setup(async function () {
  Services.telemetry.setEventRecordingEnabled("creditcard", true);
  registerCleanupFunction(async function () {
    Services.telemetry.setEventRecordingEnabled("creditcard", false);
  });
  await clearGleanTelemetry();
});

add_heuristic_tests([
  {
    description: `All fields are in the main frame`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <p><label>Card Name: <input id="cc-name" autocomplete="cc-name"></label></p>
      <p><label>Card Expiry: <input id="cc-exp" autocomplete="cc-exp"></label></p>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
    async onTestComplete() {
      assertGleanTelemetry([
        {
          event_name: "iframeLayoutDetection",
          expected_extra: {
            category: "creditcard",
            iframe_count: 0,
            main_frame: "cc-number,cc-name,cc-exp",
            iframe: "",
            cross_origin: "",
            sandboxed: "",
          },
        },
      ]);
      await clearGleanTelemetry();
    },
  },
  {
    description: `All fields are in the same same-origin iframe`,
    fixtureData: `<iframe src=${SAME_ORIGIN_ALL_FIELDS}></iframe>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
    async onTestComplete() {
      assertGleanTelemetry([
        {
          event_name: "iframeLayoutDetection",
          expected_extra: {
            category: "creditcard",
            iframe_count: 1,
            main_frame: "",
            iframe: "cc-number,cc-name,cc-exp",
            cross_origin: "",
            sandboxed: "",
          },
        },
      ]);
      await clearGleanTelemetry();
    },
  },
  {
    description: `Mix main-frame, same-origin iframe, and cross-origin iframe`,
    fixtureData: `
      <p><label>Card Number: <input id="cc-number" autocomplete="cc-number"></label></p>
      <iframe src=\"${SAME_ORIGIN_CC_NAME}\"></iframe>
      <iframe src=\"${CROSS_ORIGIN_CC_EXP}\"></iframe>
      <iframe sandbox src=\"${CROSS_ORIGIN_CC_TYPE}\"></iframe>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-type" },
        ],
      },
    ],
    async onTestComplete() {
      assertGleanTelemetry([
        {
          event_name: "iframeLayoutDetection",
          expected_extra: {
            category: "creditcard",
            iframe_count: 3,
            main_frame: "cc-number",
            iframe: "cc-name,cc-exp,cc-type",
            cross_origin: "cc-exp,cc-type",
            sandboxed: "cc-type",
          },
        },
      ]);
      await clearGleanTelemetry();
    },
  },
  {
    description: "Test category",
    fixtureData: `
      <form>
        <input id="name" autocomplete="name" />
        <input id="country" autocomplete="country"/>
        <input id="street-address" autocomplete="street-address" />
      </form>
      <form>
        <input id="cc-name" autocomplete="cc-name" />
        <input id="cc-number" autocomplete="cc-number"/>
        <input id="cc-exp" autocomplete="cc-exp" />
      </form>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "name" },
          { fieldName: "country" },
          { fieldName: "street-address" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-name" },
          { fieldName: "cc-number" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
    async onTestComplete() {
      assertGleanTelemetry(
        [
          {
            event_name: "iframeLayoutDetection",
            event_count: 2,
            expected_extra: [
              {
                category: "address",
                iframe_count: 0,
                main_frame: "name,country,street-address",
                iframe: "",
                cross_origin: "",
                sandboxed: "",
              },
              {
                category: "creditcard",
                iframe_count: 0,
                main_frame: "cc-name,cc-number,cc-exp",
                iframe: "",
                cross_origin: "",
                sandboxed: "",
              },
            ],
          },
        ],
        2
      );
      await clearGleanTelemetry();
    },
  },
]);
