// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %AsyncIterator.prototype% methods close the iterator if callback throws.
info: |
  AsyncIterator Helpers proposal 2.1.6
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
class TestError extends Error {}
class TestAsyncIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 1};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

function fn() {
  throw new TestError();
}
const methods = [
  iter => iter.map(fn),
  iter => iter.filter(fn),
  iter => iter.flatMap(fn),
];

for (const method of methods) {
  const iter = new TestAsyncIterator();
  assert.sameValue(iter.closed, false);
  method(iter).next().then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => {
      assert.sameValue(err instanceof TestError, true);
      assert.sameValue(iter.closed, true);
    },
  );
}


reportCompare(0, 0);
