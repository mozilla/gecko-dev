/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module has math utility functions around clustering from a list of vectors
 */

const JUST_BELOW_1 = 0.99999;

/**
 * Utility function to measure accuracy and related metrics for a classification
 * task based on values from a confusion matrix.
 *
 * @param {object} confusionMatrix - An object containing the counts of classification outcomes.
 * @param {int[]} confusionMatrix.truePositives - Array of true positive counts.
 * @param {int[]} confusionMatrix.trueNegatives - Array of true negative counts.
 * @param {int[]} confusionMatrix.falsePositives - Array of false positive counts.
 * @param {int[]} confusionMatrix.falseNegatives - Array of false negative counts.
 * @returns {{accuracy: number, kappa: number}} An object containing accuracy and kappa values.
 */
export function getAccuracyStats({
  truePositives,
  trueNegatives,
  falsePositives,
  falseNegatives,
}) {
  const accuracy =
    (truePositives + trueNegatives) /
    (truePositives + trueNegatives + falsePositives + falseNegatives);
  const kappa = cohenKappa2x2({
    truePositives,
    falseNegatives,
    falsePositives,
    trueNegatives,
  });
  return { accuracy, kappa };
}

/**
 * Calculate Cohen's Kappa for a 2x2 confusion matrix.
 *
 * @param {object} confusionMatrix - An object containing the confusion matrix values.
 * @param {number} confusionMatrix.truePositives - True Positives (Actual Positive, Predicted Positive).
 * @param {number} confusionMatrix.falseNegatives - False Negatives (Actual Positive, Predicted Negative).
 * @param {number} confusionMatrix.falsePositives - False Positives (Actual Negative, Predicted Positive).
 * @param {number} confusionMatrix.trueNegatives - True Negatives (Actual Negative, Predicted Negative).
 * @returns {number} The Cohen's Kappa coefficient.
 */
export function cohenKappa2x2({
  truePositives,
  falseNegatives,
  falsePositives,
  trueNegatives,
}) {
  // Total number of samples
  const total = truePositives + falseNegatives + falsePositives + trueNegatives;
  if (total === 0) {
    return 0;
  }

  // Observed agreement (p0)
  const p0 = (truePositives + trueNegatives) / total;

  // Marginal sums (row and column totals)
  const row1 = truePositives + falseNegatives;
  const row2 = falsePositives + trueNegatives;
  const col1 = truePositives + falsePositives;
  const col2 = falseNegatives + trueNegatives;

  // Expected agreement (pe)
  const pe = (row1 * col1 + row2 * col2) / (total * total);

  // Cohen's Kappa
  return (p0 - pe) / (1 - pe);
}

/**
 * Compute inner product (dot product) of two vectors
 *
 * @param {number[]} point1 Array of numbers
 * @param  {number[]} point2 Array of numbers of same dimension
 * @returns {number} The inner product
 */
export function dotProduct(point1, point2) {
  return point1.reduce((acc, value, idx) => acc + value * point2[idx], 0);
}

/**
 * Find euclidean distance between two normalized points
 *
 * @param {number[]} point1 Array of numbers
 * @param  {number[]} point2 Array of numbers of same dimension
 * @returns {number}
 */
export function euclideanDistanceNormalized(point1, point2) {
  // Optimization described here https://math.stackexchange.com/a/2981910
  const dot = dotProduct(point1, point2);
  if (dot >= JUST_BELOW_1) {
    // Prevents edge case of square root of negative number
    return 0;
  }
  return Math.sqrt(2 - 2 * dot);
}

/**
 * Find average of two vectors
 *
 * @param {number[][]} vectors List of vectors
 * @returns {number[]} The mean of the vectors
 */
export function vectorMean(vectors) {
  if (vectors.length === 0) {
    return [];
  }
  const dims = vectors[0].length;
  const sum = new Array(dims).fill(0);
  vectors.forEach(a => {
    for (let i = 0; i < dims; i++) {
      sum[i] += a[i];
    }
  });
  return sum.map(a => a / vectors.length);
}

/**
 * Normalize a vector
 *
 * @param {number[]} vector The vector to normalize
 * @returns {number[]} The normalized vector
 */
export function vectorNormalize(vector) {
  // Calculate the magnitude of the vector
  const magnitude = Math.sqrt(vector.reduce((sum, c) => sum + c ** 2, 0));
  if (magnitude === 0) {
    return vector.map(() => 0); // Return a vector of zeros
  }
  return vector.map(c => c / magnitude);
}

/**
 * Find average distance between point and a list of points in a cluster.
 * If the point referenced is in the cluster it may be excluded
 *
 * @param {number[]} point
 * @param {number[][]} clusterPoints List of vectors
 * @param {boolean} excludeSelf Exclude point that is the same point (reference equality) from the averate
 * @returns {number} The mean distance
 */
export function meanDistance(point, clusterPoints, excludeSelf) {
  let totalDistance = 0;
  let count = 0;
  for (let i = 0; i < clusterPoints.length; i++) {
    // Exclude the point itself when calculating intra-cluster distance
    if (excludeSelf && clusterPoints[i] === point) {
      // This tests reference equality, not same values
      continue;
    }
    totalDistance += euclideanDistance(point, clusterPoints[i]);
    count++;
  }
  return count > 0 ? totalDistance / count : 0;
}

/**
 * Returns silhouette coefficients of each point in all clusters, returning as a list.
 *
 * @param {number[][]} data List of vectors
 * @param {number[][]} clusterRef List of clusters, each a list of references to points
 * @returns {number[]} List of silhouette scores per point
 */
export function silhouetteCoefficients(data, clusterRef) {
  const silhouetteScores = [];
  // Group points into clusters based on labels
  const clusters = clusterRef.map(clusterIds =>
    clusterIds.map(pointID => data[pointID])
  );

  // We need a label array where each index has a label referencing a cluster
  const labels = new Array(data.length).fill(0);
  for (let q = 0; q < clusterRef.length; q++) {
    clusterRef[q].forEach(a => {
      labels[a] = q;
    });
  }
  // Iterate through each point to compute its silhouette coefficient
  for (let i = 0; i < data.length; i++) {
    const point = data[i];
    const clusterLabel = labels[i];
    const ownCluster = clusters[clusterLabel];

    if (ownCluster.length === 1) {
      // Single item cluster has a score of 0
      silhouetteScores.push(0);
      continue;
    }
    // Compute the mean intra-cluster distance (a), excluding the point itself
    const a = meanDistance(point, ownCluster, true);
    // Compute the mean nearest-cluster distance (b)
    let b = Infinity;
    for (let label = 0; label < clusterRef.length; label++) {
      if (label !== clusterLabel) {
        const nearestClusterData = clusters[label];
        const distanceToCluster = meanDistance(
          point,
          nearestClusterData,
          false
        );
        if (distanceToCluster < b) {
          b = distanceToCluster;
        }
      }
    }
    // Compute silhouette score for the current point
    const max = Math.max(a, b);
    const silhouette = max > 0 ? (b - a) / max : 0.0;
    silhouetteScores.push(silhouette);
  }
  return silhouetteScores;
}

/**
 * Performs K-Means clustering with K-Means++ initialization of centroids.
 * If an existing cluster is specified with `anchorIndices`, then one of the centroids
 * is the average of the embeddings of the items in the cluster.
 *
 * @param {object} params - An object containing the parameters for K-Means clustering.
 * @param {number[][]} params.data - The dataset represented as an array of numerical vectors.
 * @param {number} params.k - The number of clusters.
 * @param {number} params.maxIterations - The maximum number of iterations.
 * @param {Function} [params.randomFunc=Math.random] - A randomization function that returns values between 0 and 1.
 * @param {int[]} [params.anchorIndices=[]] - Indices of items that are already in a cluster.
 * @param {int[]} [params.preassignedIndices=[]] - Indices of items with predefined cluster assignments.
 * @param {boolean} [params.freezeAnchorsInZeroCluster=true] - Whether to keep anchored indices fixed in the first cluster.
 * @returns {number[][]} A list of clusters with assigned indices.
 */
export function kmeansPlusPlus({
  data,
  k,
  maxIterations,
  randomFunc = Math.random,
  anchorIndices = [],
  preassignedIndices = [],
  freezeAnchorsInZeroCluster = true,
}) {
  randomFunc = randomFunc || randomGenerator;
  maxIterations = maxIterations || 300; // Default value for maxIterations if not provided
  const dimensions = data[0].length;
  const centroids = initializeCentroidsSorted({
    X: data,
    k,
    randomFunc,
    anchorIndices,
  });
  let resultClusters = [];
  const anchorSet = new Set(anchorIndices);
  const preassignedSet = new Set(preassignedIndices);
  for (let iter = 0; iter < maxIterations; iter++) {
    resultClusters = Array.from({ length: k }, () => []);
    let hasChanged = false;

    // Assign each data point to the nearest centroid
    for (let i = 0; i < data.length; i++) {
      if (freezeAnchorsInZeroCluster && anchorSet.has(i)) {
        resultClusters[0].push(i);
      } else {
        const point = data[i];
        const centroidIndex = getClosestCentroid(
          point,
          centroids,
          preassignedSet.has(i) && !anchorSet.has(i) ? 0 : -1
        );
        resultClusters[centroidIndex].push(i); // Push index instead of the point
      }
    }
    // Recompute centroids
    for (let j = 0; j < k; j++) {
      const newCentroid = computeCentroid(resultClusters[j], data, dimensions);
      if (!arePointsEqual(centroids[j], newCentroid)) {
        centroids[j] = newCentroid;
        hasChanged = true;
      }
    }
    // Stop if centroids don't change
    if (!hasChanged) {
      break;
    }
  }
  return resultClusters;
}

/**
 * Cumulative sum for an array
 *
 * @param {number[]} arr Array of numbers
 * @returns {number[]} List of numbers of the same size, with each a cumulative sum of the previous
 * plus the new number.
 */
export function stableCumsum(arr) {
  let sum = 0;
  return arr.map(value => (sum += value));
}

/**
 * Find distances from a single point to a list of points
 *
 * @param {number[]} point A 2d array
 * @param {number[][]} X List of points (nested 2d array)
 * @returns {number[][]} List of distances
 */
export function euclideanDistancesSquared(point, X) {
  return X.map(row =>
    row.reduce((distSq, val, i) => distSq + Math.pow(val - point[i], 2), 0)
  );
}

/**
 * Generates a random number between 0 and 1
 *
 * @returns {number}
 */
function randomGenerator() {
  return Math.random();
}

/**
 * Kmeans++ initialization of centroids by finding ones farther than one another
 * See https://dl.acm.org/doi/10.5555/1283383.1283494 for details as well as Scikit learn
 * immplementation
 *
 * @param {object} params - An object containing the parameters for centroid initialization.
 * @param {number[][]} params.X - The dataset represented as an array of numerical vectors.
 * @param {number} params.k - The number of clusters (i.e., k).
 * @param {Function} params.randomFunc - A random generator function (e.g., Math.random).
 * @param {number} params.numTrials - The number of trials for initialization.
 * @param {int[]} [params.anchorIndices=[]] - Indices of items that belong to an existing group.
 * @returns {number[][]} A list of `k` initialized cluster centers.
 */
export function initializeCentroidsSorted({
  X,
  k,
  randomFunc,
  numTrials,
  anchorIndices = [],
}) {
  const nSamples = X.length;
  const nFeatures = X[0].length;
  const centers = Array.from({ length: k }, () => new Array(nFeatures).fill(0));
  numTrials = numTrials || 2 + Math.floor(Math.log(k));

  const zeroOutAnchorItems = arr => {
    // We don't want to pick other items
    anchorIndices.forEach(a => (arr[a] = 0));
  };

  // First center is random unless anchor is specified
  let centerId;
  if (anchorIndices.length <= 1) {
    if (anchorIndices.length === 1) {
      centerId = anchorIndices[0];
    } else {
      centerId = Math.floor(randomFunc() * nSamples);
    }
    centers[0] = X[centerId].slice();
  } else {
    centers[0] = vectorNormalize(vectorMean(anchorIndices.map(a => X[a])));
  }

  // Get closest distances
  const closestDistSq = euclideanDistancesSquared(centers[0], X);
  let sumOfDistances = closestDistSq.reduce(function (sum, dist) {
    return sum + dist;
  }, 0);

  // Pick the remaining nClusters-1 points
  for (let c = 1; c < k; c++) {
    // Choose center candidates by sampling
    const randVals = Array(numTrials)
      .fill(0)
      .map(() => randomFunc() * sumOfDistances);
    const closestDistSqForSamples = closestDistSq.slice();
    if (anchorIndices.length > 1) {
      // Don't pick items from anchor cluster. We already have it
      zeroOutAnchorItems(closestDistSqForSamples);
    }
    const cumulativeProbs = stableCumsum(closestDistSqForSamples);
    const candidateIds = randVals
      .map(randVal => searchSorted(cumulativeProbs, randVal))
      .filter(candId => candId < nSamples); // TODO: Filter should not be needed if searchSorted works correctly
    // Compute distances to center candidates
    const distancesToCandidates = candidateIds.map(candidateId =>
      euclideanDistancesSquared(X[candidateId], X)
    );
    // Update closest distances squared and potential for each candidate
    const candidatesSumOfDistances = distancesToCandidates.map(distances =>
      closestDistSq.reduce(
        (sum, dist, j) => sum + Math.min(dist, distances[j]),
        0
      )
    );
    // Choose the best candidate
    const bestCandidateIdx = candidatesSumOfDistances.reduce(
      (bestIdx, pot, i) =>
        pot < candidatesSumOfDistances[bestIdx] ? i : bestIdx,
      0
    );
    const bestCandidate = candidateIds[bestCandidateIdx];
    // Update closest distance and potential
    for (let i = 0; i < closestDistSq.length; i++) {
      closestDistSq[i] = Math.min(
        closestDistSq[i],
        distancesToCandidates[bestCandidateIdx][i]
      );
    }
    sumOfDistances = candidatesSumOfDistances[bestCandidateIdx];
    // Pick best candidate
    centers[c] = X[bestCandidate].slice(); // Copy array
  }
  return centers;
}

/**
 * Binary search
 *
 * @param {number[]} arr List of sorted values (ascending)
 * @param {number} val Value to search for. Return lower index if no exact match
 * @returns {number} Index of found value
 */
export function searchSorted(arr, val) {
  let low = 0;
  let high = arr.length;
  while (low < high) {
    const mid = Math.floor((low + high) / 2);
    if (arr[mid] < val) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low;
}

/**
 * Gets the centroid from a list of vectors
 *
 * @param {number[][]} vectors
 * @returns {number[]}
 */
export function computeCentroidFrom2DArray(vectors) {
  const numVectors = vectors.length;
  if (numVectors === 0) {
    return null;
  }
  const dimension = vectors[0].length;
  let centroid = new Array(dimension).fill(0);
  for (let i = 0; i < numVectors; i++) {
    for (let j = 0; j < dimension; j++) {
      centroid[j] += vectors[i][j];
    }
  }
  // Divide each component by the number of vectors to get the average
  for (let j = 0; j < dimension; j++) {
    centroid[j] /= numVectors;
  }
  return centroid;
}

/**
 * Helper function to compute Euclidean distance between two points
 *
 * @param {number[]} point1
 * @param {number[]} point2
 * @param {boolean} squareResult Return the square of the distance (i.e. don't apply SQRT)
 * @returns {number}
 */
export function euclideanDistance(point1, point2, squareResult) {
  const sum = point1.reduce(
    (acc, val, i) => acc + Math.pow(val - point2[i], 2),
    0
  );
  return squareResult ? sum : Math.sqrt(sum);
}

/**
 * Helper function to find closest centroid for a given point
 *
 * @param {number[]} point List of vectors
 * @param {number[][]} centroids List of centroids of same dimension as point
 * @param {number} excludeIndex Index of centroid to exclude from consideration
 * @returns {number} Index of the closest centroid
 */
export function getClosestCentroid(point, centroids, excludeIndex = -1) {
  var minDistance = Infinity;
  var closestIndex = -1;

  for (var i = 0; i < centroids.length; i++) {
    var distance = euclideanDistance(point, centroids[i]);
    if (distance < minDistance && i !== excludeIndex) {
      minDistance = distance;
      closestIndex = i;
    }
  }

  return closestIndex;
}

/**
 * Compute centroid of a cluster by reference
 *
 * @param {number[][]} cluster List of clusters, each with a list of indices
 * @param {number[][]} data List of all points
 * @param {number} dimensions
 * @returns {number[]}
 */
function computeCentroid(cluster, data, dimensions) {
  if (cluster.length === 0) {
    return new Array(dimensions).fill(0);
  }
  let centroid = new Array(dimensions).fill(0);
  for (let j = 0; j < cluster.length; j++) {
    let point = data[cluster[j]]; // Use the index to get the actual point
    for (let i = 0; i < dimensions; i++) {
      centroid[i] += point[i];
    }
  }
  for (let p = 0; p < dimensions; p++) {
    centroid[p] /= cluster.length;
  }
  return centroid;
}

/**
 * Returns true if both points have equal values
 *
 * @param {number[]} point1 Array of floats
 * @param {number[]} point2
 * @returns {boolean} points are equal
 */
function arePointsEqual(point1, point2) {
  return point1.every((value, index) => value === point2[index]);
}

/**
 * Computes a non-adjusted Rand score from a list of items, each with a label (true cluster id)
 * and cluster (predicted cluster id)
 *
 * @param {[object]} combinedItems
 * @param {string} clusterKey Key for predicted cluster in items
 * @param {string} labelKey Key for labeled (true) cluster in items
 * @returns {number}
 */
export function computeRandScore(combinedItems, clusterKey, labelKey) {
  let agreement = 0;
  let disagreement = 0;
  for (let i = 0; i < combinedItems.length; i++) {
    for (let j = i + 1; j < combinedItems.length; j++) {
      const itemA = combinedItems[i];
      const itemB = combinedItems[j];
      const predictedSame = itemA[clusterKey] === itemB[clusterKey];
      const labeledSame = itemA[labelKey] === itemB[labelKey];
      if (predictedSame === labeledSame) {
        agreement += 1;
      } else {
        disagreement += 1;
      }
    }
  }
  const total_items = agreement + disagreement;
  if (!total_items) {
    return 1.0;
  }
  return agreement / total_items;
}
