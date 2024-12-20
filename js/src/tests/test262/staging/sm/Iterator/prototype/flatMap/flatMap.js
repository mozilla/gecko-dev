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

let iter = [1, 2, 3].values().flatMap(x => [x, x + 1]);
for (const v of [1, 2, 2, 3, 3, 4]) {
  let result = iter.next();
  assert.sameValue(result.done, false);
  assert.sameValue(result.value, v);
}
assert.sameValue(iter.next().done, true);


reportCompare(0, 0);
