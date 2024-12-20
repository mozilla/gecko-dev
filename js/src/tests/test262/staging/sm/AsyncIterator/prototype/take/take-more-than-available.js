// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.take returns if the iterator is done.
info: |
  Iterator Helpers proposal 2.1.6.4 2. Repeat,
    ...
    c. Let next be ? Await(? IteratorNext(iterated, lastValue)).
    d. If ? IteratorComplete(next) is false, return undefined.
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
  const iter = gen([1, 2]).take(10);
  for (const expected of [1, 2]) {
    const result = await iter.next();
    assert.sameValue(result.value, expected);
    assert.sameValue(result.done, false);
  }
  const result = await iter.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);
})();


reportCompare(0, 0);
