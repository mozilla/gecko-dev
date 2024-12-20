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
for (var u = 0; u < 3; ++u) {
    var y = [];
    Object.create(y);
    gc();
    y.t = 3;
    gc();
}


reportCompare(0, 0);
