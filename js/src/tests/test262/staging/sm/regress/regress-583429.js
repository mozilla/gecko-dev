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
assert.sameValue(Object.getOwnPropertyNames(Array.prototype).indexOf("length") >= 0, true);

assert.sameValue("ok", "ok", "bug 583429");

reportCompare(0, 0);
