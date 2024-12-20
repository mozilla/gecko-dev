// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %AsyncIterator.prototype% methods throw if `next.done` throws.
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
    return {
      get done() {
        throw new TestError();
      }
    };
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

async function* gen(x) { yield x; }
const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => x),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

for (const method of methods) {
  const iterator = new TestAsyncIterator();
  assert.sameValue(iterator.closed, false);
  method(iterator).next().then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => {
      assert.sameValue(err instanceof TestError, true);
      assert.sameValue(iterator.closed, false);
    },
  );
}


reportCompare(0, 0);
