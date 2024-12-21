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
function f(y) {
    eval("{ let z=2, w=y; (function () { w.p = 7; })(); }");
}
var x = {};
f(x);
assert.sameValue(x.p, 7);

reportCompare(0, 0);
