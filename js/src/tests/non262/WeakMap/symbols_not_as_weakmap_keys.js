/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */
// SKIP test262 export
// Conflicts with https://github.com/tc39/proposal-symbols-as-weakmap-keys

var m = new WeakMap;
var sym = Symbol();
assertThrowsInstanceOf(() => m.set(sym, 0), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
