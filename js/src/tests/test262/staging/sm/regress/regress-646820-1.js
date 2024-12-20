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
    var [x, y] = [1, function () { return x; }];
    assert.sameValue(y(), 1);
})();


reportCompare(0, 0);
