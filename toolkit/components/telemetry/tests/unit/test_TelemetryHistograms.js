/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INT_MAX = 0x7fffffff;

function check_histogram(glean_name, histogram_type, name, min, max) {
  function add(val) {
    let metric = Glean.testOnlyIpc[glean_name];
    if (metric.accumulate) {
      metric.accumulate(val);
    } else {
      metric.accumulateSingleSample(val);
    }
  }
  var h = Telemetry.getHistogramById(name);
  add(0);
  var s = h.snapshot();
  Assert.equal(0, s.sum);

  var hgrams = Telemetry.getSnapshotForHistograms("main", false).parent;
  let gh = hgrams[name];
  Assert.equal(gh.histogram_type, histogram_type);

  Assert.deepEqual(gh.range, [min, max]);

  // Check that booleans work with nonboolean histograms
  add(false);
  add(true);
  s = Object.values(h.snapshot().values);
  Assert.deepEqual(s, [2, 1, 0]);

  // Check that clearing works.
  h.clear();
  s = h.snapshot();
  Assert.deepEqual(s.values, {});
  Assert.equal(s.sum, 0);

  add(0);
  add(1);
  var c = Object.values(h.snapshot().values);
  Assert.deepEqual(c, [1, 1, 0]);
}

// This MUST be the very first test of this file.
add_task(
  {
    skip_if: () => gIsAndroid,
  },
  function test_instantiate() {
    const ID = "TELEMETRY_TEST_COUNT";
    let h = Telemetry.getHistogramById(ID);

    // Instantiate the subsession histogram through |add| and make sure they match.
    // This MUST be the first use of "TELEMETRY_TEST_COUNT" in this file, otherwise
    // |add| will not instantiate the histogram.
    Glean.testOnlyIpc.aCounterForHgram.add();
    let snapshot = h.snapshot();
    let subsession = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;
    Assert.ok(ID in subsession);
    Assert.equal(
      snapshot.sum,
      subsession[ID].sum,
      "Histogram and subsession histogram sum must match."
    );
    // Clear the histogram, so we don't void the assumptions from the other tests.
    h.clear();
  }
);

add_task(async function test_parameterChecks() {
  let kinds = [Telemetry.HISTOGRAM_EXPONENTIAL, Telemetry.HISTOGRAM_LINEAR];
  let testNames = ["TELEMETRY_TEST_EXPONENTIAL", "TELEMETRY_TEST_LINEAR"];
  let gleanNames = ["aTimingDist", "aMemoryDist"];
  for (let i = 0; i < kinds.length; i++) {
    let histogram_type = kinds[i];
    let test_type = testNames[i];
    let glean_name = gleanNames[i];
    let [min, max, bucket_count] = [1, INT_MAX - 1, 10];
    check_histogram(
      glean_name,
      histogram_type,
      test_type,
      min,
      max,
      bucket_count
    );
  }
});

add_task(async function test_noSerialization() {
  // Instantiate the storage for this histogram and make sure it doesn't
  // get reflected into JS, as it has no interesting data in it.
  Telemetry.getHistogramById("NEWTAB_PAGE_PINNED_SITES_COUNT");
  let histograms = Telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.equal(false, "NEWTAB_PAGE_PINNED_SITES_COUNT" in histograms);
});

add_task(async function test_boolean_histogram() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var r = h.snapshot().range;
  // boolean histograms ignore numeric parameters
  Assert.deepEqual(r, [1, 2]);

  Glean.testOnlyIpc.aLabeledCounterForHgram.true.add();
  Glean.testOnlyIpc.aLabeledCounterForHgram.false.add();
  var s = h.snapshot();
  Assert.equal(s.histogram_type, Telemetry.HISTOGRAM_BOOLEAN);
  Assert.deepEqual(s.values, { 0: 1, 1: 1, 2: 0 });
  Assert.equal(s.sum, 1);
});

add_task(async function test_count_histogram() {
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  h.clear();
  let s = h.snapshot();
  Assert.deepEqual(s.range, [1, 2]);
  Assert.deepEqual(s.values, {});
  Assert.equal(s.sum, 0);
  Glean.testOnlyIpc.aCounterForHgram.add();
  s = h.snapshot();
  Assert.deepEqual(s.values, { 0: 1, 1: 0 });
  Assert.equal(s.sum, 1);
  Glean.testOnlyIpc.aCounterForHgram.add();
  s = h.snapshot();
  Assert.deepEqual(s.values, { 0: 2, 1: 0 });
  Assert.equal(s.sum, 2);
});

add_task(async function test_categorical_histogram() {
  let h = Telemetry.getHistogramById("TELEMETRY_TEST_CATEGORICAL_OPTOUT");
  for (let v of [
    "CommonLabel",
    "CommonLabel",
    "Label4",
    "Label5",
    "Label6",
    0,
    1,
  ]) {
    Glean.testOnlyIpc.aLabeledCounterForCategorical[v].add();
  }
  for (let s of ["", "Label3", "1234"]) {
    // Should not throw for unexpected values.
    Glean.testOnlyIpc.aLabeledCounterForCategorical[s].add();
  }

  let snapshot = h.snapshot();
  Assert.equal(snapshot.sum, 6);
  Assert.deepEqual(snapshot.range, [1, 50]);
  Assert.deepEqual(snapshot.values, { 0: 2, 1: 1, 2: 1, 3: 1, 4: 0 });
});

add_task(async function test_getCategoricalLabels() {
  let h = Telemetry.getCategoricalLabels();

  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL, [
    "CommonLabel",
    "Label2",
    "Label3",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL_OPTOUT, [
    "CommonLabel",
    "Label4",
    "Label5",
    "Label6",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_CATEGORICAL_NVALUES, [
    "CommonLabel",
    "Label7",
    "Label8",
  ]);
  Assert.deepEqual(h.TELEMETRY_TEST_KEYED_CATEGORICAL, [
    "CommonLabel",
    "Label2",
    "Label3",
  ]);
});

add_task(async function test_getHistogramById() {
  try {
    Telemetry.getHistogramById("nonexistent");
    do_throw("This can't happen");
  } catch (e) {}
  var h = Telemetry.getHistogramById("CYCLE_COLLECTOR");
  var s = h.snapshot();
  Assert.equal(s.histogram_type, Telemetry.HISTOGRAM_EXPONENTIAL);
  Assert.deepEqual(s.range, [1, 10000]);
});

add_task(async function test_getSlowSQL() {
  var slow = Telemetry.slowSQL;
  Assert.ok("mainThread" in slow && "otherThreads" in slow);
});

// Check that telemetry doesn't record in private mode
add_task(async function test_privateMode() {
  var h = Telemetry.getHistogramById("TELEMETRY_TEST_BOOLEAN");
  var orig = h.snapshot();
  Telemetry.canRecordExtended = false;
  Glean.testOnlyIpc.aLabeledCounterForHgram.true.add();
  Assert.deepEqual(orig, h.snapshot());
  Telemetry.canRecordExtended = true;
  Glean.testOnlyIpc.aLabeledCounterForHgram.true.add();
  Assert.notDeepEqual(orig, h.snapshot());
});

add_task(async function test_expired_histogram() {
  var test_expired_id = "TELEMETRY_TEST_EXPIRED";
  Glean.testOnly.expiredHist.accumulateSingleSample(1);

  for (let process of ["main", "content", "gpu", "extension"]) {
    let histograms = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    );
    if (!(process in histograms)) {
      info("Nothing present for process " + process);
      continue;
    }
    Assert.equal(histograms[process].__expired__, undefined);
  }
  let parentHgrams = Telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  Assert.equal(parentHgrams[test_expired_id], undefined);
});

add_task(async function test_keyed_histogram() {
  // Check that invalid names get rejected.

  let threw = false;
  try {
    Telemetry.getKeyedHistogramById(
      "test::unknown histogram",
      "never",
      Telemetry.HISTOGRAM_BOOLEAN
    );
  } catch (e) {
    // This should throw as it is an unknown ID
    threw = true;
  }
  Assert.ok(threw, "getKeyedHistogramById should have thrown");
});

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_clearHistogramsOnSnapshot() {
    const COUNT = "TELEMETRY_TEST_COUNT";
    let h = Telemetry.getHistogramById(COUNT);
    h.clear();
    let snapshot;

    // The first snapshot should be empty, nothing recorded.
    snapshot = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;
    Assert.ok(!(COUNT in snapshot));

    // After recording into a histogram, the data should be in the snapshot. Don't delete it.
    Glean.testOnlyIpc.aCounterForHgram.add(1);

    Assert.equal(h.snapshot().sum, 1);
    snapshot = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;
    Assert.ok(COUNT in snapshot);
    Assert.equal(snapshot[COUNT].sum, 1);

    // After recording into a histogram again, the data should be updated and in the snapshot.
    // Clean up after.
    Glean.testOnlyIpc.aCounterForHgram.add(41);

    Assert.equal(h.snapshot().sum, 42);
    snapshot = Telemetry.getSnapshotForHistograms(
      "main",
      true /* clear */
    ).parent;
    Assert.ok(COUNT in snapshot);
    Assert.equal(snapshot[COUNT].sum, 42);

    // Finally, no data should be in the snapshot.
    Assert.equal(h.snapshot().sum, 0);
    snapshot = Telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;
    Assert.ok(!(COUNT in snapshot));
  }
);

add_task(async function test_can_record_in_process_regression_bug_1530361() {
  Telemetry.getSnapshotForHistograms("main", true);

  // The socket and gpu processes should not have any histograms.
  // Flag and count histograms have defaults, so if we're accidentally recording them
  // in these processes they'd show up even immediately after being cleared.
  let snapshot = Telemetry.getSnapshotForHistograms("main", true);

  Assert.deepEqual(
    snapshot.gpu,
    {},
    "No histograms should have been recorded for the gpu process"
  );
  Assert.deepEqual(
    snapshot.socket,
    {},
    "No histograms should have been recorded for the socket process"
  );
});

add_task(function test_knows_its_name() {
  let h;

  // Plain histograms
  const histNames = [
    "TELEMETRY_TEST_COUNT",
    "TELEMETRY_TEST_CATEGORICAL",
    "TELEMETRY_TEST_EXPIRED",
  ];

  for (let name of histNames) {
    h = Telemetry.getHistogramById(name);
    Assert.equal(name, h.name());
  }

  // Keyed histograms
  const keyedHistNames = [
    "TELEMETRY_TEST_KEYED_EXPONENTIAL",
    "TELEMETRY_TEST_KEYED_BOOLEAN",
    "TELEMETRY_TEST_EXPIRED_KEYED",
  ];

  for (let name of keyedHistNames) {
    h = Telemetry.getKeyedHistogramById(name);
    Assert.equal(name, h.name());
  }
});
