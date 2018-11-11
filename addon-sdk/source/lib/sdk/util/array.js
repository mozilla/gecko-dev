/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

/**
 * Returns `true` if given `array` contain given `element` or `false`
 * otherwise.
 * @param {Array} array
 *    Target array.
 * @param {Object|String|Number|Boolean} element
 *    Element being looked up.
 * @returns {Boolean}
 */
var has = exports.has = function has(array, element) {
  // shorter and faster equivalent of `array.indexOf(element) >= 0`
  return !!~array.indexOf(element);
};
var hasAny = exports.hasAny = function hasAny(array, elements) {
  if (arguments.length < 2)
    return false;
  if (!Array.isArray(elements))
    elements = [ elements ];
  return array.some(function (element) {
      return has(elements, element);
  });
};

/**
 * Adds given `element` to the given `array` if it does not contain it yet.
 * `true` is returned if element was added otherwise `false` is returned.
 * @param {Array} array
 *    Target array.
 * @param {Object|String|Number|Boolean} element
 *    Element to be added.
 * @returns {Boolean}
 */
var add = exports.add = function add(array, element) {
  var result;
  if ((result = !has(array, element)))
    array.push(element);

  return result;
};

/**
 * Removes first occurrence of the given `element` from the given `array`. If
 * `array` does not contain given `element` `false` is returned otherwise
 * `true` is returned.
 * @param {Array} array
 *    Target array.
 * @param {Object|String|Number|Boolean} element
 *    Element to be removed.
 * @returns {Boolean}
 */
exports.remove = function remove(array, element) {
  var result;
  if ((result = has(array, element)))
    array.splice(array.indexOf(element), 1);

  return result;
};

/**
 * Produces a duplicate-free version of the given `array`.
 * @param {Array} array
 *    Source array.
 * @returns {Array}
 */
function unique(array) {
  return array.reduce(function(result, item) {
    add(result, item);
    return result;
  }, []);
};
exports.unique = unique;

/**
 * Produce an array that contains the union: each distinct element from all
 * of the passed-in arrays.
 */
function union() {
  return unique(Array.concat.apply(null, arguments));
};
exports.union = union;

exports.flatten = function flatten(array){
   var flat = [];
   for (var i = 0, l = array.length; i < l; i++) {
    flat = flat.concat(Array.isArray(array[i]) ? flatten(array[i]) : array[i]);
   }
   return flat;
};

function fromIterator(iterator) {
  let array = [];
  if (iterator.__iterator__) {
    for (let item of iterator)
      array.push(item);
  }
  else {
    for (let item of iterator)
      array.push(item);
  }
  return array;
}
exports.fromIterator = fromIterator;

function find(array, predicate, fallback) {
  var index = 0;
  var count = array.length;
  while (index < count) {
    var value = array[index];
    if (predicate(value)) return value;
    else index = index + 1;
  }
  return fallback;
}
exports.find = find;
