/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/
(function () {
    var [x, y] = [function () { return y; }, 13];
    assert.sameValue(x(), 13);
})();


reportCompare(0, 0);
