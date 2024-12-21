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
  yield 2;
  yield 3;
}

const log = [];
const fn = (value) => {
  log.push(value.toString());
  return value % 2 == 0;
};

gen().find(fn).then(result => {
  assert.sameValue(result, 2);
  assert.sameValue(log.join(','), '1,2');
});


reportCompare(0, 0);
