"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FeatureModel: "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
  dictAdd: "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
  dictApply: "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
  DayTimeWeighting: "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
  InterestFeatures: "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
  unaryEncodeDiffPrivacy:
    "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs",
});

add_task(function test_dictAdd() {
  let dict = {};
  dictAdd(dict, "a", 3);
  Assert.equal(dict.a, 3, "Should set value when key is missing");

  dictAdd(dict, "a", 2);
  Assert.equal(dict.a, 5, "Should add value when key exists");
});

add_task(function test_dictApply() {
  let input = { a: 1, b: 2 };
  let output = dictApply(input, x => x * 2);
  Assert.deepEqual(output, { a: 2, b: 4 }, "Should double all values");

  let identity = dictApply(input, x => x);
  Assert.deepEqual(
    identity,
    input,
    "Should return same values with identity function"
  );
});

add_task(function test_DayTimeWeighting_getDateIntervals() {
  let weighting = new DayTimeWeighting([1, 2], [0.5, 0.2]);
  let now = Date.now();
  let intervals = weighting.getDateIntervals(now);

  Assert.equal(
    intervals.length,
    2,
    "Should return one interval per pastDay entry"
  );
  Assert.ok(
    intervals[0].end <= new Date(now),
    "Each interval end should be before or equal to now"
  );
  Assert.ok(
    intervals[0].start < intervals[0].end,
    "Start should be before end"
  );
  Assert.ok(
    intervals[1].end <= new Date(now),
    "Each interval end should be before or equal to now"
  );
  Assert.ok(
    intervals[1].start < intervals[0].end,
    "Start should be before end"
  );
});

add_task(function test_DayTimeWeighting_getRelativeWeight() {
  let weighting = new DayTimeWeighting([1, 2], [0.5, 0.2]);

  Assert.equal(
    weighting.getRelativeWeight(0),
    0.5,
    "Should return correct weight for index 0"
  );
  Assert.equal(
    weighting.getRelativeWeight(1),
    0.2,
    "Should return correct weight for index 1"
  );
  Assert.equal(
    weighting.getRelativeWeight(2),
    0,
    "Should return 0 for out-of-range index"
  );
});

add_task(function test_DayTimeWeighting_fromJSON() {
  const json = { days: [1, 2], relative_weight: [0.1, 0.3] };
  const weighting = DayTimeWeighting.fromJSON(json);

  Assert.ok(
    weighting instanceof DayTimeWeighting,
    "Should create instance from JSON"
  );
  Assert.deepEqual(
    weighting.pastDays,
    [1, 2],
    "Should correctly parse pastDays"
  );
  Assert.deepEqual(
    weighting.relativeWeight,
    [0.1, 0.3],
    "Should correctly parse relative weights"
  );
});

add_task(function test_InterestFeatures_applyThresholds() {
  let feature = new InterestFeatures("test", {}, [10, 20, 30]);
  // Note that number of output is 1 + the length of the input weights
  Assert.equal(
    feature.applyThresholds(5),
    0,
    "Value < first threshold returns 0"
  );
  Assert.equal(
    feature.applyThresholds(15),
    1,
    "Value < second threshold returns 1"
  );
  Assert.equal(
    feature.applyThresholds(25),
    2,
    "Value < third threshold returns 2"
  );
  Assert.equal(
    feature.applyThresholds(35),
    3,
    "Value >= all thresholds returns length of thresholds"
  );
});

add_task(function test_InterestFeatures_noThresholds() {
  let feature = new InterestFeatures("test", {});
  Assert.equal(
    feature.applyThresholds(42),
    42,
    "Without thresholds, should return input unchanged"
  );
});

add_task(function test_InterestFeatures_fromJSON() {
  const json = { features: { a: 1 }, thresholds: [1, 2] };
  const feature = InterestFeatures.fromJSON("f", json);

  Assert.ok(
    feature instanceof InterestFeatures,
    "Should create InterestFeatures from JSON"
  );
  Assert.equal(feature.name, "f", "Should set correct name");
  Assert.deepEqual(
    feature.featureWeights,
    { a: 1 },
    "Should set correct feature weights"
  );
  Assert.deepEqual(feature.thresholds, [1, 2], "Should set correct thresholds");
});

const SPECIAL_FEATURE_CLICK = "clicks";

const AggregateResultKeys = {
  POSITION: "position",
  FEATURE: "feature",
  VALUE: "feature_value",
  SECTION_POSITION: "section_position",
  FORMAT_ENUM: "card_format_enum",
};

const SCHEMA = {
  [AggregateResultKeys.FEATURE]: 0,
  [AggregateResultKeys.FORMAT_ENUM]: 1,
  [AggregateResultKeys.VALUE]: 2,
};

const jsonModelData = {
  model_type: "clicks",
  day_time_weighting: {
    days: [3, 14, 45],
    relative_weight: [1, 0.5, 0.3],
  },
  interest_vector: {
    news_reader: {
      features: { pub_nytimes_com: 0.5, pub_cnn_com: 0.5 },
      thresholds: [0.3, 0.4, 0.5],
      diff_p: 1,
      diff_q: 0,
    },
    parenting: {
      features: { parenting: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 1,
      diff_q: 0,
    },
    [SPECIAL_FEATURE_CLICK]: {
      features: { click: 1 },
      thresholds: [10, 30],
      diff_p: 1,
      diff_q: 0,
    },
  },
};

const jsonModelDataNoCoarseSupport = {
  model_type: "clicks",
  day_time_weighting: {
    days: [3, 14, 45],
    relative_weight: [1, 0.5, 0.3],
  },
  interest_vector: {
    news_reader: {
      features: { pub_nytimes_com: 0.5, pub_cnn_com: 0.5 },
      thresholds: [],
      // MISSING thresholds
      diff_p: 1,
      diff_q: 0,
    },
    parenting: {
      features: { parenting: 1 },
      thresholds: [0.3, 0.4],
      // MISSING p,q values
    },
    [SPECIAL_FEATURE_CLICK]: {
      features: { click: 1 },
      thresholds: [10, 30],
      diff_p: 1,
      diff_q: 0,
    },
  },
};

add_task(function test_FeatureModel_fromJSON() {
  const model = FeatureModel.fromJSON(jsonModelData);
  const curTime = new Date();
  const intervals = model.getDateIntervals(curTime);
  Assert.equal(intervals.length, jsonModelData.day_time_weighting.days.length);
  for (const interval of intervals) {
    Assert.ok(
      interval.start.getTime() <= interval.end.getTime(),
      "Interval start and end are in correct order"
    );
    Assert.ok(
      interval.end.getTime() <= curTime.getTime(),
      "Interval end is not in future"
    );
  }
});

const SQL_RESULT_DATA = [
  [
    ["click", 0, 1],
    ["parenting", 0, 1],
  ],
  [
    ["click", 0, 2],
    ["parenting", 0, 1],
    ["pub_nytimes_com", 0, 1],
  ],
  [],
];

add_task(function test_modelChecks() {
  const model = FeatureModel.fromJSON(jsonModelData);
  Assert.equal(
    model.supportsCoarseInterests(),
    true,
    "Supports coarse interests check yes "
  );
  Assert.equal(
    model.supportsCoarsePrivateInterests(),
    true,
    "Supports coarse private interests check yes "
  );

  const modelNoCoarse = FeatureModel.fromJSON(jsonModelDataNoCoarseSupport);
  Assert.equal(
    modelNoCoarse.supportsCoarseInterests(),
    false,
    "Supports coarse interests check no "
  );
  Assert.equal(
    modelNoCoarse.supportsCoarsePrivateInterests(),
    false,
    "Supports coarse private interests check no "
  );
});

add_task(function test_computeInterestVector() {
  const modelData = { ...jsonModelData, rescale: true };
  const model = FeatureModel.fromJSON(modelData);
  const result = model.computeInterestVector({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    applyThresholding: false,
  });
  Assert.ok("parenting" in result, "Result should contain parenting");
  Assert.ok("news_reader" in result, "Result should contain news_reader");
  Assert.equal(result.parenting, 1.0, "Vector is rescaled");

  Assert.equal(result[SPECIAL_FEATURE_CLICK], 2, "Should include raw click");
});

add_task(function test_computeThresholds() {
  const modelData = { ...jsonModelData, rescale: true };
  const model = FeatureModel.fromJSON(modelData);
  const result = model.computeInterestVector({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    applyThresholding: true,
  });
  Assert.equal(result.parenting, 2, "Threshold is applied");
  Assert.equal(
    result[SPECIAL_FEATURE_CLICK],
    0,
    "Should include thresholded raw click"
  );
});

add_task(function test_unaryEncoding() {
  const numValues = 4;

  Assert.equal(
    unaryEncodeDiffPrivacy(0, numValues, 1, 0),
    "1000",
    "Basic dp works with out of range p, q"
  );
  Assert.equal(
    unaryEncodeDiffPrivacy(1, numValues, 1, 0),
    "0100",
    "Basic dp works with out of range p, q"
  );
  Assert.equal(
    unaryEncodeDiffPrivacy(500, numValues, 0.75, 0.25).length,
    4,
    "Basic dp runs with unexpected input"
  );
  Assert.equal(
    unaryEncodeDiffPrivacy(-100, numValues, 0.75, 0.25).length,
    4,
    "Basic dp runs with unexpected input"
  );
  Assert.equal(
    unaryEncodeDiffPrivacy(1, numValues, 0.75, 0.25).length,
    4,
    "Basic dp runs with typical values"
  );
  Assert.equal(
    unaryEncodeDiffPrivacy(1, numValues, 0.8, 0.6).length,
    4,
    "Basic dp runs with typical values"
  );
});

add_task(function test_differentialPrivacy() {
  const modelData = { ...jsonModelData, rescale: true };
  const model = FeatureModel.fromJSON(modelData);
  const result = model.computeInterestVector({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    applyThresholding: true,
    applyDifferentialPrivacy: true,
  });
  Assert.equal(
    result.parenting,
    "001",
    "Threshold is applied with differential privacy"
  );
  Assert.equal(result[SPECIAL_FEATURE_CLICK].length, 3, "Apply DP to clicks");
});

add_task(function test_computeMultipleVectors() {
  const modelData = { ...jsonModelData, rescale: true };
  const model = FeatureModel.fromJSON(modelData);
  const result = model.computeInterestVectors({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    model_id: "test",
    condensePrivateValues: false,
  });
  Assert.equal(
    result.coarsePrivateInferredInterests.parenting,
    "001",
    "Threshold is applied with differential privacy"
  );
  Assert.ok(
    Number.isInteger(result.coarseInferredInterests.parenting),
    "Threshold is applied for coarse interest"
  );
  Assert.ok(
    result.inferredInterests.parenting > 0,
    "Original inferred interest is returned"
  );
});

add_task(function test_computeMultipleVectorsCondensed() {
  const modelData = { ...jsonModelData, rescale: true };
  const model = FeatureModel.fromJSON(modelData);
  const result = model.computeInterestVectors({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    model_id: "test",
  });
  Assert.equal(
    result.coarsePrivateInferredInterests.values.length,
    3,
    "Items in an array"
  );
  Assert.equal(
    result.coarsePrivateInferredInterests.values[0].length,
    3,
    "One value in string per possible result"
  );
  Assert.ok(
    result.coarsePrivateInferredInterests.values[0]
      .split("")
      .every(a => a === "1" || a === "0"),
    "Combined coarse values are 1 and 0"
  );
  Assert.equal(
    result.coarsePrivateInferredInterests.model_id,
    "test",
    "Model id returned"
  );
  Assert.ok(
    result.inferredInterests.parenting > 0,
    "Original inferred interest is returned"
  );
});

add_task(function test_computeMultipleVectorsNoPrivate() {
  const model = FeatureModel.fromJSON(jsonModelDataNoCoarseSupport);
  const result = model.computeInterestVectors({
    dataForIntervals: SQL_RESULT_DATA,
    indexSchema: SCHEMA,
    model_id: "test",
    condensePrivateValues: false,
  });
  Assert.ok(
    !result.coarsePrivateInferredInterests,
    "No coarse private interests available"
  );
  Assert.ok(!result.coarseInferredInterests, "No coarse interests available");
  Assert.ok(
    result.inferredInterests.parenting > 0,
    "Original inferred interest is returned"
  );
});

add_task(function test_computeCTRInterestVectorsNoNoise() {
  const model = new FeatureModel({
    modelId: "test-ctr-model",
    interestVectorModel: {},
    modelType: "ctr",
    noiseScale: 0.0,
    dayTimeWeighting: null,
    tileImportance: null,
    rescale: true,
    logScale: false,
  });

  const clicks = { sports: 1, news: 2 };
  const impressions = { sports: 4, news: 4 };

  const result = model.computeCTRInterestVectors(
    clicks,
    impressions,
    "test-ctr-model"
  );
  Assert.equal(result.model_id, "test-ctr-model", "Model id is CTR");
  Assert.ok(
    Math.abs(result.sports - 0.25) <= 1e-4,
    "CTR model result is as expected"
  );
  Assert.ok(
    Math.abs(result.news - 0.5) <= 1e-4,
    "CTR model result is as expected"
  );
});

add_task(function test_computCTRInterestVectorsWithNoise() {
  const model = new FeatureModel({
    modelId: "test-ctr-model",
    interestVectorModel: {},
    modelType: "ctr",
    noiseScale: 1.0,
    laplaceNoiseFn: () => 0.42, // deterministically inject noise
  });

  const clicks = { sports: 1, news: 2, science: 10 };
  const impressions = { sports: 4, news: 4, science: 11 };

  const result = model.computeCTRInterestVectors(
    clicks,
    impressions,
    "test-ctr-model"
  );

  // Assert the stubbed noise is added
  Assert.equal(result.sports, 1 / 4 + 0.42, "sports CTR + noise");
  Assert.equal(result.news, 2 / 4 + 0.42, "news CTR + noise");
  Assert.equal(result.science, 10 / 11 + 0.42, "science CTR + noise");
  Assert.equal(result.model_id, "test-ctr-model", "model ID is correct");
});
