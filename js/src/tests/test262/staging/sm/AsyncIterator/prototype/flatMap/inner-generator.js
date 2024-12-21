// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.flatMap innerIterator can be a generator.
info: |
  Iterator Helpers proposal 2.1.6.7
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
async function* gen() {
  yield 1;
  yield 2;
}

(async () => {
  const iter = gen().flatMap(async function*(x) {
    yield x;
    yield* [x + 1, x + 2];
  });

  for (const expected of [1, 2, 3, 2, 3, 4]) {
    const result = await iter.next();
    assert.sameValue(result.value, expected);
    assert.sameValue(result.done, false);
  }

  const result = await iter.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);
})();


reportCompare(0, 0);
