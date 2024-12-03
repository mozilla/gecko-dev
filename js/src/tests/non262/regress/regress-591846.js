/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// SKIP test262 export
// Note that the values for enumerable/configurable are inconsistent with
// https://github.com/tc39/proposal-regexp-legacy-features

// Reset RegExp.leftContext to the empty string.
/x/.test('x');

var d = Object.getOwnPropertyDescriptor(RegExp, "leftContext");
assertEq(d.set, undefined);
assertEq(typeof d.get, "function");
assertEq(d.enumerable, true);
assertEq(d.configurable, false);
assertEq(d.get.call(RegExp), "");

reportCompare(0, 0, "ok");
