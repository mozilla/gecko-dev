/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test the firefox-on-glean profiler integration. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { ProfilerTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ProfilerTestUtils.sys.mjs"
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_setup(
  /* on Android FOG is set up through the global xpcshell head.js */
  { skip_if: () => AppConstants.platform == "android" },
  function test_setup() {
    // FOG needs a profile directory to put its data in.
    do_get_profile();

    // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
    Services.fog.initializeFOG();
  }
);

/**
 * Start the profiler, run `func`, stop the profiler, and get a collection of
 * markers that were recorded while running `func`.
 *
 * @param {string} type The marker payload type, e.g. "Counter"
 * @param {object} func The function that runs glean code to generate markers
 * @returns {object} The markers generated during `func`, with the id field
 *  expanded using the string table
 */
async function runWithProfilerAndGetMarkers(type, func) {
  await ProfilerTestUtils.startProfiler({
    entries: 10000,
    interval: 10,
    features: ["nostacksampling"],
    threads: ["GeckoMain"],
  });

  Assert.ok(Services.profiler.IsActive());

  await func();

  let profile = await ProfilerTestUtils.stopNowAndGetProfile();

  // We assume that we only have one thread being profiled here.
  Assert.equal(
    profile.threads.length,
    1,
    "We should only be profiling one thread"
  );

  let markers = ProfilerTestUtils.getPayloadsOfType(profile.threads[0], type);
  let stringTable = profile.threads[0].stringTable;

  // We expect that the id, or name, of a marker should be a unique string.
  // Go through them and look up the values so that we can just write a string
  // in the test, and not use a numerical id (which may not be stable!)
  for (let marker of markers) {
    marker.id = stringTable[marker.id];
  }

  // Return selected markers
  return markers;
}

add_task(async function test_fog_counter_markers() {
  let markers = await runWithProfilerAndGetMarkers("IntLikeMetric", () => {
    Glean.testOnly.badCode.add(31);
  });

  Assert.deepEqual(markers, [
    {
      type: "IntLikeMetric",
      id: "testOnly.badCode",
      val: 31,
    },
  ]);
});

add_task(async function test_fog_string_markers() {
  const value = "a cheesy string!";
  let markers = await runWithProfilerAndGetMarkers("StringLikeMetric", () => {
    Glean.testOnly.cheesyString.set(value);
    Glean.testOnly.cheesyString.set("a".repeat(2048));
  });

  // Test that we correctly truncate long strings:
  const truncatedLongString = "a".repeat(1024);

  Assert.deepEqual(markers, [
    {
      type: "StringLikeMetric",
      id: "testOnly.cheesyString",
      val: value,
    },
    {
      type: "StringLikeMetric",
      id: "testOnly.cheesyString",
      val: truncatedLongString,
    },
  ]);
});

add_task(async function test_fog_string_list() {
  const value = "a cheesy string!";
  const value2 = "a cheesier string!";
  const value3 = "the cheeziest of strings.";

  const cheeseList = [value, value2];

  let markers = await runWithProfilerAndGetMarkers("StringLikeMetric", () => {
    Glean.testOnly.cheesyStringList.set(cheeseList);
    Glean.testOnly.cheesyStringList.add(value3);
  });

  Assert.deepEqual(markers, [
    {
      type: "StringLikeMetric",
      id: "testOnly.cheesyStringList",
      // Note: This is a little fragile and will need to be updated if we ever
      // rearrange the items in the string list.
      val: `[${value},${value2}]`,
    },
    {
      type: "StringLikeMetric",
      id: "testOnly.cheesyStringList",
      val: value3,
    },
  ]);
});

add_task(async function test_fog_timespan() {
  let markers = await runWithProfilerAndGetMarkers(
    "TimespanMetric",
    async () => {
      Glean.testOnly.canWeTimeIt.start();
      Glean.testOnly.canWeTimeIt.cancel();

      // We start, briefly sleep and then stop.
      // That guarantees some time to measure.
      Glean.testOnly.canWeTimeIt.start();
      await sleep(10);
      Glean.testOnly.canWeTimeIt.stop();

      // Set a raw value to make sure we get values in markers
      Glean.testOnly.canWeTimeIt.setRaw(100);
    }
  );

  Assert.deepEqual(markers, [
    { type: "TimespanMetric", id: "testOnly.canWeTimeIt" },
    { type: "TimespanMetric", id: "testOnly.canWeTimeIt" },
    { type: "TimespanMetric", id: "testOnly.canWeTimeIt" },
    { type: "TimespanMetric", id: "testOnly.canWeTimeIt" },
    { type: "TimespanMetric", id: "testOnly.canWeTimeIt", val: 100 },
  ]);
});

add_task(
  async function test_fog_timespan_throws_on_stop_wout_start_but_still_records() {
    let markers = await runWithProfilerAndGetMarkers("TimespanMetric", () => {
      // Throws an error inside glean, but we still expect to see a marker
      Glean.testOnly.canWeTimeIt.stop();
    });

    Assert.deepEqual(markers, [
      { type: "TimespanMetric", id: "testOnly.canWeTimeIt" },
    ]);
  }
);

add_task(async function test_fog_uuid() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  let generatedUuid;
  let markers = await runWithProfilerAndGetMarkers("StringLikeMetric", () => {
    Glean.testOnly.whatIdIt.set(kTestUuid);

    // We need to compare the generated UUID to what we record in a marker. Since
    // this won't be stable across test runs (we can't "write it down"), we have
    // to instead query glean for what it generated, and then check that that
    // value is in the marker.
    Glean.testOnly.whatIdIt.generateAndSet();
    generatedUuid = Glean.testOnly.whatIdIt.testGetValue("test-ping");
  });

  Assert.deepEqual(markers, [
    {
      type: "StringLikeMetric",
      id: "testOnly.whatIdIt",
      val: kTestUuid,
    },
    {
      type: "StringLikeMetric",
      id: "testOnly.whatIdIt",
      val: generatedUuid,
    },
  ]);
});

add_task(async function test_fog_datetime() {
  const value = new Date();

  let markers = await runWithProfilerAndGetMarkers("DatetimeMetric", () => {
    Glean.testOnly.whatADate.set(value.getTime() * 1000);
  });

  Assert.deepEqual(markers, [
    {
      type: "DatetimeMetric",
      id: "testOnly.whatADate",
      time: value.toISOString(),
    },
  ]);
});

add_task(async function test_fog_boolean_markers() {
  let markers = await runWithProfilerAndGetMarkers("BooleanMetric", () => {
    Glean.testOnly.canWeFlagIt.set(false);
    Glean.testOnly.canWeFlagIt.set(true);
  });

  Assert.deepEqual(markers, [
    {
      type: "BooleanMetric",
      id: "testOnly.canWeFlagIt",
      val: false,
    },
    {
      type: "BooleanMetric",
      id: "testOnly.canWeFlagIt",
      val: true,
    },
  ]);
});

add_task(async function test_fog_event_markers() {
  let markers = await runWithProfilerAndGetMarkers("EventMetric", () => {
    // Record an event that produces a marker with `extra` undefined:
    Glean.testOnlyIpc.noExtraEvent.record();

    let extra = { extra1: "can set extras", extra2: "passing more data" };
    Glean.testOnlyIpc.anEvent.record(extra);

    // Corner case: Event with extra with `undefined` value.
    // Should pretend that the extra key (extra1) isn't there.
    let extraWithUndef = { extra1: undefined, extra2: "defined" };
    Glean.testOnlyIpc.anEvent.record(extraWithUndef);

    let extra2 = {
      extra1: "can set extras",
      extra2: 37,
      extra3_longer_name: false,
    };
    Glean.testOnlyIpc.eventWithExtra.record(extra2);

    // camelCase extras work.
    let extra5 = {
      extra4CamelCase: false,
    };
    Glean.testOnlyIpc.eventWithExtra.record(extra5);

    // Passing `null` works.
    Glean.testOnlyIpc.eventWithExtra.record(null);

    // Invalid extra keys don't crash, the event is not recorded,
    // but an error and marker are recorded.
    let extra3 = {
      extra1_nonexistent_extra: "this does not crash",
    };
    Glean.testOnlyIpc.eventWithExtra.record(extra3);

    // Supplying extras when there aren't any defined results in the event not
    // being recorded, but an error is, along with a marker
    Glean.testOnlyIpc.noExtraEvent.record(extra3);
  });

  let expected_markers = [
    {
      type: "EventMetric",
      id: "testOnlyIpc.noExtraEvent",
    },
    {
      type: "EventMetric",
      id: "testOnlyIpc.anEvent",
      extra: { extra1: "can set extras", extra2: "passing more data" },
    },
    {
      type: "EventMetric",
      id: "testOnlyIpc.anEvent",
      extra: { extra2: "defined" },
    },
    {
      type: "EventMetric",
      id: "testOnlyIpc.eventWithExtra",
      extra: {
        extra3_longer_name: "false",
        extra2: "37",
        extra1: "can set extras",
      },
    },
    {
      type: "EventMetric",
      id: "testOnlyIpc.eventWithExtra",
      extra: { extra4CamelCase: "false" },
    },
    {
      type: "EventMetric",
      id: "testOnlyIpc.eventWithExtra",
    },
    // This event throws an error in glean, but we still record a marker
    {
      type: "EventMetric",
      id: "testOnlyIpc.eventWithExtra",
      extra: { extra1_nonexistent_extra: "this does not crash" },
    },
    // This event throws an error in glean, but we still record a marker
    {
      type: "EventMetric",
      id: "testOnlyIpc.noExtraEvent",
      extra: { extra1_nonexistent_extra: "this does not crash" },
    },
  ];

  // Parse the `extra` field of each marker into a JS object so that we can do
  // a deep equality check, ignoring undefined extras.
  markers.forEach(m => {
    if (m.extra !== undefined) {
      m.extra = JSON.parse(m.extra);
    }
  });

  Assert.deepEqual(markers, expected_markers);
});

add_task(async function test_fog_memory_distribution() {
  let markers = await runWithProfilerAndGetMarkers("DistMetric", () => {
    Glean.testOnly.doYouRemember.accumulate(7);
    Glean.testOnly.doYouRemember.accumulate(17);
    // Note, we would like to test something like this, to ensure that we test
    // the internal `accumulate_samples` marker, but the JS API doesn't support
    // accumulating multiple samples (or an array of samples).
    // Glean.testOnly.doYouRemember.accumulate([17, 2134, 543]);
  });

  // We need to filter the markers to *just* the ones that we care about in this
  // test, as otherwise we get a number of *actual* memory metric markers
  // (i.e. "performanceCloneDeserialize.<x>") in the list of markers that we
  // want to check. As we can't really predict these, we therefore can't "write
  // down" what they will be, and we therefore can't write down a "gold" value
  // to compare against.
  let testMarkers = markers.filter(
    marker => marker.id == "testOnly.doYouRemember"
  );

  Assert.deepEqual(testMarkers, [
    {
      type: "DistMetric",
      id: "testOnly.doYouRemember",
      sample: 7,
    },
    {
      type: "DistMetric",
      id: "testOnly.doYouRemember",
      sample: 17,
    },
  ]);
});

add_task(async function test_fog_custom_distribution() {
  let markers = await runWithProfilerAndGetMarkers("DistMetric", () => {
    Glean.testOnlyIpc.aCustomDist.accumulateSingleSample(120);
    Glean.testOnlyIpc.aCustomDist.accumulateSamples([7, 268435458]);

    // Negative values will not be recorded, instead an error is recorded.
    // However, we still expect to see a marker!
    Glean.testOnlyIpc.aCustomDist.accumulateSamples([-7]);
  });

  // As with memory distribution markers, we also need to filter markers here,
  // as they are both recorded using the `DistMetric` schema.
  let testMarkers = markers.filter(
    marker => marker.id == "testOnlyIpc.aCustomDist"
  );

  Assert.deepEqual(testMarkers, [
    {
      type: "DistMetric",
      id: "testOnlyIpc.aCustomDist",
      sample: 120,
    },
    {
      type: "DistMetric",
      id: "testOnlyIpc.aCustomDist",
      samples: "[7,268435458]",
    },
    {
      type: "DistMetric",
      id: "testOnlyIpc.aCustomDist",
      samples: "[-7]",
    },
  ]);
});

add_task(async function test_fog_timing_distribution() {
  let markers = await runWithProfilerAndGetMarkers("TimingDist", async () => {
    let t1 = Glean.testOnly.whatTimeIsIt.start();
    let t2 = Glean.testOnly.whatTimeIsIt.start();

    await sleep(5);

    let t3 = Glean.testOnly.whatTimeIsIt.start();
    Glean.testOnly.whatTimeIsIt.cancel(t1);

    await sleep(5);

    Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t2); // 10ms
    Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t3); // 5ms
    // samples are measured in microseconds, since that's the unit listed in metrics.yaml
    Glean.testOnly.whatTimeIsIt.accumulateSingleSample(5000); // 5ms
    Glean.testOnly.whatTimeIsIt.accumulateSamples([2000, 8000]); // 10ms
  });

  // Again, we need to filter markers here, as we also time how long it takes to
  // clone/deserialise items in the JS interpreter, which are reported using
  // the `TimingDist` marker schema.
  let testMarkers = markers.filter(
    marker => marker.id == "testOnly.whatTimeIsIt"
  );

  Assert.deepEqual(testMarkers, [
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 1 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 2 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 3 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 1 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 2 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", timer_id: 3 },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", sample: "5000" },
    { type: "TimingDist", id: "testOnly.whatTimeIsIt", samples: "[2000,8000]" },
  ]);
});

add_task(async function test_fog_quantity() {
  let markers = await runWithProfilerAndGetMarkers("IntLikeMetric", () => {
    Glean.testOnly.meaningOfLife.set(42);
  });
  Assert.deepEqual(markers, [
    { type: "IntLikeMetric", id: "testOnly.meaningOfLife", val: 42 },
  ]);
});

add_task(async function test_fog_rate() {
  let markers = await runWithProfilerAndGetMarkers("IntLikeMetric", () => {
    // 1) Standard rate with internal denominator
    Glean.testOnlyIpc.irate.addToNumerator(22);
    Glean.testOnlyIpc.irate.addToDenominator(7);

    // 2) Rate with external denominator
    Glean.testOnlyIpc.anExternalDenominator.add(11);
    Glean.testOnlyIpc.rateWithExternalDenominator.addToNumerator(121);
  });

  Assert.deepEqual(markers, [
    { type: "IntLikeMetric", id: "testOnlyIpc.irate", val: 22 },
    { type: "IntLikeMetric", id: "testOnlyIpc.irate", val: 7 },
    { type: "IntLikeMetric", id: "testOnlyIpc.anExternalDenominator", val: 11 },
    {
      type: "IntLikeMetric",
      id: "testOnlyIpc.rateWithExternalDenominator",
      val: 121,
    },
  ]);
});

add_task(async function test_fog_url() {
  const value = "https://www.example.com/fog";
  let markers = await runWithProfilerAndGetMarkers("UrlMetric", () => {
    Glean.testOnlyIpc.aUrl.set(value);
  });

  Assert.deepEqual(markers, [
    {
      type: "UrlMetric",
      id: "testOnlyIpc.aUrl",
      val: value,
    },
  ]);
});

add_task(async function test_fog_text() {
  const value =
    "Before the risin' sun, we fly, So many roads to choose, We'll start out walkin' and learn to run, (We've only just begun)";
  let markers = await runWithProfilerAndGetMarkers("StringLikeMetric", () => {
    Glean.testOnlyIpc.aText.set(value);
  });
  Assert.deepEqual(markers, [
    { type: "StringLikeMetric", id: "testOnlyIpc.aText", val: value },
  ]);
});

add_task(async function test_fog_text_unusual_character() {
  const value =
    "The secret to Dominique Ansel's viennoiserie is the use of Isigny Sainte-Mère butter and Les Grands Moulins de Paris flour";
  let markers = await runWithProfilerAndGetMarkers("StringLikeMetric", () => {
    Glean.testOnlyIpc.aText.set(value);
  });

  Assert.deepEqual(markers, [
    { type: "StringLikeMetric", id: "testOnlyIpc.aText", val: value },
  ]);
});

add_task(async function test_fog_object_markers() {
  if (!Glean.testOnly.balloons) {
    // FIXME(bug 1883857): object metric type not available, e.g. in artifact builds.
    // Skipping this test.
    return;
  }
  let markers = await runWithProfilerAndGetMarkers("ObjectMetric", () => {
    let balloons = [
      { colour: "red", diameter: 5 },
      { colour: "blue", diameter: 7 },
      { colour: "orange" },
    ];
    Glean.testOnly.balloons.set(balloons);

    // These values are coerced to null or removed.
    balloons = [
      { colour: "inf", diameter: Infinity },
      { colour: "negative-inf", diameter: -1 / 0 },
      { colour: "nan", diameter: NaN },
      { colour: "undef", diameter: undefined },
    ];
    Glean.testOnly.balloons.set(balloons);

    // colour != color.
    // This is invalid, but still produces a marker!
    let invalid = [{ color: "orange" }, { color: "red", diameter: "small" }];
    Glean.testOnly.balloons.set(invalid);

    Services.fog.testResetFOG();

    // set again to ensure it's stored
    balloons = [
      { colour: "red", diameter: 5 },
      { colour: "blue", diameter: 7 },
    ];
    Glean.testOnly.balloons.set(balloons);

    // Again, invalid, but produces a marker
    invalid = [{ colour: "red", diameter: 5, extra: "field" }];
    Glean.testOnly.balloons.set(invalid);

    // More complex objects:
    Glean.testOnly.crashStack.set({});

    let stack = {
      status: "OK",
      crash_info: {
        typ: "main",
        address: "0xf001ba11",
        crashing_thread: 1,
      },
      main_module: 0,
      modules: [
        {
          base_addr: "0x00000000",
          end_addr: "0x00004000",
        },
      ],
    };

    Glean.testOnly.crashStack.set(stack);

    stack = {
      status: "OK",
      modules: [
        {
          base_addr: "0x00000000",
          end_addr: "0x00004000",
        },
      ],
    };
    Glean.testOnly.crashStack.set(stack);

    stack = {
      status: "OK",
      modules: [],
    };
    Glean.testOnly.crashStack.set(stack);

    stack = {
      status: "OK",
    };
    Glean.testOnly.crashStack.set(stack);
  });

  let expected_markers = [
    {
      type: "ObjectMetric",
      id: "testOnly.balloons",
      value: [
        { colour: "red", diameter: 5 },
        { colour: "blue", diameter: 7 },
        { colour: "orange" },
      ],
    },
    // Check that values are coerced or removed
    {
      type: "ObjectMetric",
      id: "testOnly.balloons",
      value: [
        { colour: "inf", diameter: null },
        { colour: "negative-inf", diameter: null },
        { colour: "nan", diameter: null },
        { colour: "undef" },
      ],
    },
    // Invalid glean object, but still produces a marker
    {
      type: "ObjectMetric",
      id: "testOnly.balloons",
      value: [{ color: "orange" }, { color: "red", diameter: "small" }],
    },

    {
      type: "ObjectMetric",
      id: "testOnly.balloons",
      value: [
        { colour: "red", diameter: 5 },
        { colour: "blue", diameter: 7 },
      ],
    },
    // Invalid glean object, but still produces a marker
    {
      type: "ObjectMetric",
      id: "testOnly.balloons",
      value: [{ colour: "red", diameter: 5, extra: "field" }],
    },

    {
      type: "ObjectMetric",
      id: "testOnly.crashStack",
      value: {},
    },

    {
      type: "ObjectMetric",
      id: "testOnly.crashStack",
      value: {
        status: "OK",
        crash_info: {
          typ: "main",
          address: "0xf001ba11",
          crashing_thread: 1,
        },
        main_module: 0,
        modules: [{ base_addr: "0x00000000", end_addr: "0x00004000" }],
      },
    },
    {
      type: "ObjectMetric",
      id: "testOnly.crashStack",
      value: {
        status: "OK",
        modules: [{ base_addr: "0x00000000", end_addr: "0x00004000" }],
      },
    },
    // Modules gets erased within Glean, but it still shows up in a marker
    {
      type: "ObjectMetric",
      id: "testOnly.crashStack",
      value: { status: "OK", modules: [] },
    },

    {
      type: "ObjectMetric",
      id: "testOnly.crashStack",
      value: { status: "OK" },
    },
  ];

  // Parse the `value` field of each marker into a JS object so that we can do
  // a deep equality check, ignoring undefined values.
  markers.forEach(m => {
    if (m.value !== undefined) {
      m.value = JSON.parse(m.value);
    }
  });

  Assert.deepEqual(markers, expected_markers);
});
