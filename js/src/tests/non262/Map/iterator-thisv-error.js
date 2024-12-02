/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

function test(fn, thisv) {
  assertThrowsInstanceOfWithMessageCheck(
    () => fn.call(thisv),
    TypeError,
    message =>
      /^\w+ method called on incompatible.+/.test(message) && !message.includes("std_"));
}

for (var thisv of [null, undefined, false, true, 0, ""]) {
  test(Map.prototype.values, thisv);
  test(Map.prototype.keys, thisv);
  test(Map.prototype.entries, thisv);
  test(Map.prototype[Symbol.iterator], thisv);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
