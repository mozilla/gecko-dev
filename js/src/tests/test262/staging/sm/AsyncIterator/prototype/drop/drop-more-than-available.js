// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.drop returns if the iterator is done.
info: |
  Iterator Helpers proposal 2.1.6.5 1. Repeat, while remaining > 0,
    ...
    b. Let next be ? Await(? IteratorStep(iterated)).
    c. If ? IteratorComplete(next) is true, return undefined.
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
class TestIterator extends AsyncIterator {
  counter = 0;
  async next() {
    return {done: ++this.counter >= 2, value: undefined};
  }
}

(async () => {
  let iter = [1, 2].values().drop(3);
  let result = await iter.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);

  iter = new TestIterator();
  let dropped = iter.drop(10);
  result = await dropped.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);
  assert.sameValue(iter.counter, 2);
})();


reportCompare(0, 0);
