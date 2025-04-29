/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Portions of this file are originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

// Used only to cause test failures.

var pSlice = Array.prototype.slice;

export var ObjectUtils = {
  /**
   * This tests objects & values for deep equality.
   *
   * We check using the most exact approximation of equality between two objects
   * to keep the chance of false positives to a minimum.
   * `JSON.stringify` is not designed to be used for this purpose; objects may
   * have ambiguous `toJSON()` implementations that would influence the test.
   *
   * @param {any} a
   *   Object or value to be compared.
   * @param {any} b
   *   Object or value to be compared.
   */
  deepEqual(a, b) {
    return _deepEqual(a, b);
  },

  /**
   * Returns `true` if `obj` is an array without elements, an object without
   * enumerable properties, or a falsy primitive; `false` otherwise.
   *
   * @param {any} obj
   */
  isEmpty(obj) {
    if (!obj) {
      return true;
    }
    if (typeof obj != "object") {
      return false;
    }
    if (Array.isArray(obj)) {
      return !obj.length;
    }
    for (let key in obj) {
      return false;
    }
    return true;
  },
};

// ... Start of previously MIT-licensed code.
// This deepEqual implementation is originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

/**
 * Tests objects & values for deep equality.
 *
 * @param {any} a
 * @param {any} b
 */
function _deepEqual(a, b) {
  // The numbering below refers to sections in the CommonJS spec.

  // 7.1 All identical values are equivalent, as determined by ===.
  if (a === b) {
    return true;
    // 7.2 If the b value is a Date object, the a value is
    // equivalent if it is also a Date object that refers to the same time.
  }
  let aIsDate = instanceOf(a, "Date");
  let bIsDate = instanceOf(b, "Date");
  if (aIsDate || bIsDate) {
    if (!aIsDate || !bIsDate) {
      return false;
    }
    if (isNaN(a.getTime()) && isNaN(b.getTime())) {
      return true;
    }
    return a.getTime() === b.getTime();
    // 7.3 If the b value is a RegExp object, the a value is
    // equivalent if it is also a RegExp object with the same source and
    // properties (`global`, `multiline`, `lastIndex`, `ignoreCase`).
  }
  let aIsRegExp = instanceOf(a, "RegExp");
  let bIsRegExp = instanceOf(b, "RegExp");
  if (aIsRegExp || bIsRegExp) {
    return (
      aIsRegExp &&
      bIsRegExp &&
      a.source === b.source &&
      a.global === b.global &&
      a.multiline === b.multiline &&
      a.lastIndex === b.lastIndex &&
      a.ignoreCase === b.ignoreCase
    );
    // 7.4 Other pairs that do not both pass typeof value == "object",
    // equivalence is determined by ==.
  }
  if (typeof a != "object" || typeof b != "object") {
    return a == b;
  }
  // 7.5 For all other Object pairs, including Array objects, equivalence is
  // determined by having the same number of owned properties (as verified
  // with Object.prototype.hasOwnProperty.call), the same set of keys
  // (although not necessarily the same order), equivalent values for every
  // corresponding key, and an identical 'prototype' property. Note: this
  // accounts for both named and indexed properties on Arrays.
  return objEquiv(a, b);
}

/**
 * Tests to see if an object is a particular instance of the given type.
 *
 * @param {object} object
 * @param {string} type
 */
function instanceOf(object, type) {
  return Object.prototype.toString.call(object) == "[object " + type + "]";
}

/**
 * Checks is see if the value is undefined or null.
 *
 * @param {any} value
 */
function isUndefinedOrNull(value) {
  return value === null || value === undefined;
}

/**
 * Checks to see if the object is an arguments object.
 *
 * @param {object} object
 */
function isArguments(object) {
  return instanceOf(object, "Arguments");
}

/**
 * Compares objects for equivalence.
 *
 * @param {object} a
 * @param {object} b
 * @returns {boolean}
 */
function objEquiv(a, b) {
  if (isUndefinedOrNull(a) || isUndefinedOrNull(b)) {
    return false;
  }
  // An identical 'prototype' property.
  if ((a.prototype || undefined) != (b.prototype || undefined)) {
    return false;
  }

  // Check for ArrayBuffer equality
  if (instanceOf(a, "ArrayBuffer") && instanceOf(b, "ArrayBuffer")) {
    if (a.byteLength !== b.byteLength) {
      return false;
    }
    const viewA = new Uint8Array(a);
    const viewB = new Uint8Array(b);
    for (let i = 0; i < viewA.length; i++) {
      if (viewA[i] !== viewB[i]) {
        return false;
      }
    }
    return true;
  }

  // Object.keys may be broken through screwy arguments passing. Converting to
  // an array solves the problem.
  if (isArguments(a)) {
    if (!isArguments(b)) {
      return false;
    }
    a = pSlice.call(a);
    b = pSlice.call(b);
    return _deepEqual(a, b);
  }
  let ka, kb;
  try {
    ka = Object.keys(a);
    kb = Object.keys(b);
  } catch (e) {
    // Happens when one is a string literal and the other isn't
    return false;
  }
  // Having the same number of owned properties (keys incorporates
  // hasOwnProperty)
  if (ka.length != kb.length) {
    return false;
  }
  // The same set of keys (although not necessarily the same order),
  ka.sort();
  kb.sort();
  // Equivalent values for every corresponding key, and possibly expensive deep
  // test
  for (let key of ka) {
    if (!_deepEqual(a[key], b[key])) {
      return false;
    }
  }
  return true;
}

// ... End of previously MIT-licensed code.
