/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks if numbers are close up to decimalPoints decimal points
 *
 * @param {number} a
 * @param {number} b
 * @param {number} decimalPoints
 * @returns {boolean} True if numbers are similar
 */
function numberLooseEquals(a, b, decimalPoints = 2) {
  return a.toFixed(decimalPoints) === b.toFixed(decimalPoints);
}

/**
 * Compares two vectors up to decimalPoints decimal points
 * Returns true if all items the same up to decimalPoints threshold
 *
 * @param {number[]} a
 * @param {number[]} b
 * @param {number} decimalPoints
 * @returns {boolean} True if vectors are similar
 */
function vectorLooseEquals(a, b, decimalPoints = 2) {
  return a.every(
    (item, index) =>
      item.toFixed(decimalPoints) === b[index].toFixed(decimalPoints)
  );
}

/**
 * Extremely simple generator deterministic seeded list of numbers between
 * 0 and 1 for use of tests in place of a true random generator
 *
 * @param {number} seed
 * @returns {function(): number}
 */
function simpleNumberSequence(seed = 0) {
  const values = [
    0.42, 0.145, 0.5, 0.9234, 0.343, 0.1324, 0.8343, 0.534, 0.634, 0.3233,
  ];
  let counter = Math.floor(seed) % values.length;
  return () => {
    counter = (counter + 1) % values.length;
    return values[counter];
  };
}

/**
 * Utility function to shuffle an array, using a random
 *
 * @param {object[]} array of items to shuffle
 * @param {Function} randFunc function that returns between 0 and 1
 */
function shuffleArray(array, randFunc) {
  randFunc = randFunc ?? Math.random;
  for (let i = array.length - 1; i >= 0; i--) {
    const j = Math.floor(randFunc() * (i + 1));
    [array[i], array[j]] = [array[j], array[i]];
  }
}

/**
 * Returns dict that averages input values
 *
 * @param {object[]} itemArray List of dicts, each with values to average
 * @returns {object} Object with average of values passed in itemArray
 */
function averageStatsValues(itemArray) {
  const result = {};
  if (itemArray.length === 0) {
    return result;
  }
  for (const key of Object.keys(itemArray[0])) {
    let total = 0.0;
    itemArray.forEach(a => (total += a[key]));
    result[key] = total / itemArray.length;
  }
  return result;
}
