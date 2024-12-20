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

const log = [];
async function* gen(n) {
  log.push(`${n}`);
  yield 1;
  log.push(`${n}`);
  yield 2;
}

Promise.all([gen(1).find(() => {}), gen(2).find(() => {})]).then(
  () => {
    assert.sameValue(
      log.join(' '),
      '1 2 1 2',
    );
  },
  err => {
    throw err;
  }
);


reportCompare(0, 0);
