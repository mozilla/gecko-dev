import { FeatureModel } from "lib/InferredModel/FeatureModel.sys.mjs";

const jsonData = {
  model_id: "test",
  schema_ver: 1,
  day_time_weighting: {
    days: [3, 14, 45],
    relative_weight: [0.33, 0.33, 0.33],
  },
  interest_vector: {
    cryptosport: {
      features: { crypto: 0.5, sport: 0.5 },
      thresholds: [0.3, 0.4, 0.5],
    },
    parenting: {
      features: { parenting: 1 },
      thresholds: [0.3, 0.4],
    },
  },
};

describe("Inferred Model", () => {
  it("create model", () => {
    const model = FeatureModel.fromJSON(jsonData);
    assert.equal(model.model_id, jsonData.modelId);
  });
  it("create time intervals", () => {
    const model = FeatureModel.fromJSON(jsonData);
    assert.equal(model.model_id, jsonData.modelId);

    const curTime = new Date();
    const intervals = model.getDateIntervals(curTime);

    assert.equal(intervals.length, jsonData.day_time_weighting.days.length);
    for (const interval of intervals) {
      assert.isTrue(interval.start < curTime.getTime());
      assert.isTrue(interval.end <= curTime.getTime());
      assert.isTrue(interval.start <= interval.end);
    }
  });
});
