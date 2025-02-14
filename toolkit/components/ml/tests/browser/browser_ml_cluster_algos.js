"use strict";

/// <reference path="head.js" />

const {
  silhouetteCoefficients,
  dotProduct,
  getAccuracyStats,
  meanDistance,
  euclideanDistanceNormalized,
  euclideanDistancesSquared,
  euclideanDistance,
  stableCumsum,
  searchSorted,
  vectorMean,
  vectorNormalize,
  cohenKappa2x2,
  computeRandScore,
  initializeCentroidsSorted,
  kmeansPlusPlus,
} = ChromeUtils.importESModule(
  "chrome://global/content/ml/ClusterAlgos.sys.mjs"
);

add_task(function testEquals() {
  ok(numberLooseEquals(1, 1.001), "Loose equals number");
  ok(numberLooseEquals(1, 0.999), "Loose equals number");
  ok(!numberLooseEquals(1, 1.1), "Loose equals number");
  ok(!numberLooseEquals(1, 1.14), "Loose equals number");
  ok(vectorLooseEquals([1, 0], [1.001, 0]), "Loose equals vector");
  ok(!vectorLooseEquals([1, 0], [1.001, 0.1]), "Loose equals vector");
});

add_task(function testKappa() {
  ok(
    numberLooseEquals(
      cohenKappa2x2({
        truePositives: 10,
        trueNegatives: 10,
        falsePositives: 10,
        falseNegatives: 10,
      }),
      0
    ),
    "Kappa basic test 0"
  );

  ok(
    numberLooseEquals(
      cohenKappa2x2({
        truePositives: 10,
        trueNegatives: 10,
        falsePositives: 0,
        falseNegatives: 0,
      }),
      1
    ),
    "Kappa basic test 1"
  );
});

add_task(function testGetAccuracyStats() {
  // Test Case 1: Perfect classification (accuracy should be 1, kappa should be 1)
  let result = getAccuracyStats({
    truePositives: 10,
    trueNegatives: 10,
    falsePositives: 0,
    falseNegatives: 0,
  });

  ok(numberLooseEquals(result.accuracy, 1), "Perfect accuracy test");
  ok(numberLooseEquals(result.kappa, 1), "Perfect kappa test");

  // Test Case 2: Random classification (accuracy should be 0.5, kappa should be 0)
  result = getAccuracyStats({
    truePositives: 5,
    trueNegatives: 5,
    falsePositives: 5,
    falseNegatives: 5,
  });

  ok(
    numberLooseEquals(result.accuracy, 0.5),
    "Random classification accuracy test"
  );
  ok(numberLooseEquals(result.kappa, 0), "Random classification kappa test");

  // Test Case 3: Completely incorrect classification (accuracy should be 0, kappa should be -1)
  result = getAccuracyStats({
    truePositives: 0,
    trueNegatives: 0,
    falsePositives: 10,
    falseNegatives: 10,
  });

  ok(
    numberLooseEquals(result.accuracy, 0),
    "Completely wrong classification accuracy test"
  );
  ok(
    numberLooseEquals(result.kappa, -1),
    "Completely wrong classification kappa test"
  );
});

add_task(function testSilhouette() {
  const silScores = silhouetteCoefficients(
    [
      [1.0, 1.0],
      [0.0, 0.0],
      [3.0, 3.0],
      [0.1, 0.1],
    ],
    [[0, 3], [1], [2]]
  );
  ok(
    vectorLooseEquals(silScores, [0.1, 0, 0, -0.88888889]),
    "Silhouette score test"
  );
});

add_task(function testVectorMean() {
  ok(
    vectorLooseEquals(
      vectorMean([
        [2.0, 0.0],
        [0.0, 1.0],
      ]),
      [1.0, 0.5]
    ),
    "Mean"
  );
});

add_task(function testVectorNormalize() {
  ok(
    vectorLooseEquals(vectorNormalize([2.0, 0.0]), [1.0, 0.0]),
    "Normalize test simple"
  );
  ok(
    vectorLooseEquals(vectorNormalize([0.0, -1.0]), [0.0, -1.0]),
    "Normalize test negative"
  );
  ok(
    vectorLooseEquals(vectorNormalize([1.0, 1.0]), [0.707, 0.707]),
    "Normalize test 45 degrees"
  );
});

add_task(function testDotProduct() {
  Assert.equal(dotProduct([1.0, 0.0], [0.0, 1.0]), 0.0);
  Assert.equal(dotProduct([1.0, 0.0], [1.0, 1.0]), 1.0);
  Assert.equal(dotProduct([1.0, 0.0], [1.0, 6.0]), 1.0);
  Assert.equal(dotProduct([1.0, 0.0], [6.0, 6.0]), 6.0);
});

add_task(async function testEuclideanDistanceNormalized() {
  const dist = euclideanDistanceNormalized([1.0, 0.0], [0.0, 1.0]);
  ok(numberLooseEquals(dist, 1.414), "Euclidean distance normalized");
});

add_task(async function testEuclideanDistance() {
  const dist = euclideanDistance([1.0, 0.0], [0.0, 1.0]);
  ok(numberLooseEquals(dist, 1.414), "Euclidean distance");
  const dist2 = euclideanDistance([1.0, 0.0], [5.0, 0.0]);
  ok(numberLooseEquals(dist2, 4), "Euclidean distance");
  const dist3 = euclideanDistance([1.0, 0.0], [5.0, 0.0], true);
  ok(numberLooseEquals(dist3, 16), "Euclidean distance - squared");
});

add_task(async function testEuclideanDistancesSquared() {
  const dist = euclideanDistancesSquared(
    [0.0, 1.0],
    [
      [0.0, 1.0],
      [0, 0],
      [0, 5],
      [5, 1],
    ]
  );
  ok(vectorLooseEquals(dist, [0, 1, 16, 25]), "Euclidean distance");
});

add_task(async function testMeansDistance() {
  const selfPoint = [3.0];
  const d = meanDistance(
    selfPoint,
    [[0.0], [1.0], [2.0], selfPoint, [4.0]],
    false
  );
  const d2 = meanDistance(
    selfPoint,
    [[0.0], [1.0], [2.0], selfPoint, [4.0]],
    true
  );
  const d3 = meanDistance(selfPoint, [[3.0], [3.0]], false);
  Assert.equal(d, (3 + 2 + 1 + 1) / 5.0, "Mean distance");
  Assert.equal(d2, (3 + 2 + 1 + 1) / 4.0, "Mean distance exclude self");
  Assert.equal(d3, 0, "Mean distance - zero");
});

add_task(async function testInitializeCentroidsSorted() {
  const centroids = initializeCentroidsSorted({
    X: [
      [1, 0, 0],
      [1, 0, 0],
      [0, 1, 1],
      [0, 1, 1],
    ],
    randomFunc: simpleNumberSequence(),
    k: 2,
  });
  Assert.equal(centroids.length, 2);
});

add_task(async function testKMeansPlusPlusAllClusters() {
  const results = kmeansPlusPlus({
    data: [
      [1, 0, 0],
      [1, 0, 0],
      [0, 1, 1],
      [0, 1, 1],
    ],
    k: 2,
    maxIterations: 2,
    randomFunc: simpleNumberSequence(),
  });
  Assert.equal(results.length, 2);
});

add_task(async function testKMeansPlusPlusPinnedItem() {
  const results = kmeansPlusPlus({
    data: [
      [1, 0, 0],
      [1, 0, 0],
      [0, 1, 1],
      [0, 1, 1],
    ],
    k: 2,
    maxIterations: 2,
    randomFunc: simpleNumberSequence(),
    anchorIndices: [0],
  });
  results[0].includes(0);
  results[0].includes(1);
  results[1].includes(2);
  results[1].includes(3);
  Assert.equal(results.length, 2);
});

add_task(async function testKMeansPlusPlusPinnedItems() {
  const results = kmeansPlusPlus({
    data: [
      [1, 0, 0],
      [1, 0, 0],
      [0, 1, 1],
      [0, 1, 1],
    ],
    k: 2,
    maxIterations: 2,
    randomFunc: simpleNumberSequence(),
    anchorIndices: [0, 1],
  });
  results[0].includes(0);
  results[0].includes(1);
  results[1].includes(2);
  results[1].includes(3);
  Assert.equal(results.length, 2);
});

add_task(async function testSearchSorted() {
  Assert.equal(searchSorted([1.1, 2.1, 3.1], 2), 1, "Sorted search");
  Assert.equal(searchSorted([1.1, 2.1, 3.1], 2.2), 2, "Sorted search"); /// TODO - investigate
  Assert.equal(
    searchSorted([1.1, 2.1, 3.1], 2.1),
    1,
    "Sorted search exact match"
  );
  Assert.equal(searchSorted([1.1, 2.1, 3.1], 0), 0, "Sorted search underflow");
  Assert.equal(searchSorted([1.1, 2.1, 3.1], 5), 3, "Sorted search overflow"); // TODO - make sure edge case is handled by users
});

add_task(async function testStableCumsum() {
  ok(
    vectorLooseEquals(stableCumsum([1, 2, 3]), [1, 3, 6]),
    "Stable cumulative sum basic"
  );
  Assert.strictEqual(stableCumsum([]).length, 0, "Stable cumulative sum empty");
});

add_task(async function testComputeRandScore() {
  Assert.strictEqual(
    computeRandScore(
      [
        { A: 1, B: 1 },
        { A: 2, B: 2 },
      ],
      "A",
      "B"
    ),
    1,
    "computeRandScore basic all match"
  );
  Assert.strictEqual(stableCumsum([]).length, 0, "Stable cumulative sum empty");
});
