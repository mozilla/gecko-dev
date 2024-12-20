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
assert.sameValue(new Error().hasOwnProperty('message'), false);
assert.sameValue(new Error(undefined).hasOwnProperty('message'), false);


reportCompare(0, 0);
