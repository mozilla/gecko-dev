// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.flatMap skips empty inner iterables.
info: |
  Iterator Helpers proposal 2.1.6.7 1. Repeat,
    ...
    k. Repeat, while innerAlive is true,
      ...
      v. Let innerComplete be IteratorComplete(innerNext).
      ...
      vii. If innerComplete is true, set innerAlive to false.
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
async function* gen(values) {
  yield* values;
}

(async () => {
  let iter = gen([0, 1, 2, 3]).flatMap(x => x % 2 ? gen([]) : gen([x]));

  for (const expected of [0, 2]) {
    const result = await iter.next();
    assert.sameValue(result.value, expected);
    assert.sameValue(result.done, false);
  }

  let result = await iter.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);

  iter = gen([0, 1, 2, 3]).flatMap(x => gen([]));
  result = await iter.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);
})();


reportCompare(0, 0);
