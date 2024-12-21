// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- async-iteration
description: |
  pending
esid: pending
---*/

async function* gen() {
  yield 1;
  yield 3;
  yield 5;
}
const fn = x => x % 2 == 0;

gen().find(fn).then(result => assert.sameValue(result, undefined));

async function* empty() {}
empty().find(x => x).then(result => assert.sameValue(result, undefined));


reportCompare(0, 0);
