// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
- IsHTMLDDA
- async-iteration
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
description: |
  pending
esid: pending
---*/

async function* gen(value) {
  yield value;
}
const fn = x => x;
function check(value, expected) {
  gen(value).every(fn).then(result => assert.sameValue(result, expected));
}

check(true, true);
check(1, true);
check([], true);
check({}, true);
check('test', true);

check(false, false);
check(0, false);
check('', false);
check(null, false);
check(undefined, false);
check(NaN, false);
check(-0, false);
check(0n, false);
check(createIsHTMLDDA(), false);
check(Promise.resolve(false), false);


reportCompare(0, 0);
