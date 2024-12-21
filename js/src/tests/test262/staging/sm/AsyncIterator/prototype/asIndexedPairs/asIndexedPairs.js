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

let iter = gen().asIndexedPairs();

for (const v of [[0, 1], [1, 2], [2, 3]]) {
  iter.next().then(
    result => {
      assert.sameValue(result.done, false);
      assert.sameValue(result.value[0], v[0]);
      assert.sameValue(result.value[1], v[1]);
    }
  );
}

iter.next().then(({done}) => assert.sameValue(done, true));


reportCompare(0, 0);
