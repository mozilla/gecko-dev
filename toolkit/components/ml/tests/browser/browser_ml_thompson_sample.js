"use strict";

// const { sinon } = ChromeUtils.importESModule(
//   "resource://testing-common/Sinon.sys.mjs"
// );

// <reference path="head.js" />

const {
  thompsonSampleSort,
  sortKeysValues,
  sampleBeta,
  sampleGamma,
  sampleNormal,
} = ChromeUtils.importESModule("chrome://global/content/ml/ThomSample.sys.mjs");

const eps = 1e-10;

add_task(function test_sortKeysvalues() {
  const scores = [1.0, 2.0, 3.0];
  const keys = ["a", "b", "c"];

  const result = sortKeysValues(scores, keys);

  Assert.deepEqual(result[0], ["c", "b", "a"], "check sorted order");
  Assert.deepEqual(result[1], [3.0, 2.0, 1.0], "check sorted order");
});

function makeRandomStub(values) {
  let i = 0;
  return function () {
    if (i >= values.length) {
      throw new Error("Too many Math.random() calls!");
    }
    return values[i++];
  };
}

add_task(function test_sampleGamma() {
  const testCases = [
    {
      a: 2.0,
      x: 1.0,
      uni: 0.5,
      expected: 3.319683214263267,
    },
  ];

  for (const { a, x, uni, expected } of testCases) {
    const stubbedNormal = makeRandomStub([x]);
    const stubbedUni = makeRandomStub([uni]);
    const result = sampleGamma(a, stubbedNormal, stubbedUni);
    Assert.less(
      Math.abs(result - expected),
      eps,
      `Expected ~${expected}, got ${result}`
    );
  }
});

add_task(function test_sampleNormal() {
  const testCases = [
    {
      u: 0.85,
      vRaw: 0.453,
      expected: -0.09486258823529409,
    },
  ];

  for (const { u, vRaw, expected } of testCases) {
    const stubbedRandom = makeRandomStub([u, vRaw]);
    const result = sampleNormal(stubbedRandom);
    Assert.less(
      Math.abs(result - expected),
      eps,
      `Expected ~${expected}, got ${result}`
    );
  }
});
