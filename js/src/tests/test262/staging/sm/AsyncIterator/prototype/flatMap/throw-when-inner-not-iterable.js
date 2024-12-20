// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.flatMap closes the iterator and throws when mapped isn't iterable.
info: |
  Iterator Helpers proposal 2.1.6.7 1. Repeat,
    ...
    h. Let innerIterator be GetIterator(mapped, async).
    i. IfAbruptCloseAsyncIterator(innerIterator, iterated).
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
class NotIterable {
  async next() {
    return {done: true};
  }
}

class InvalidIterable {
  [Symbol.asyncIterator]() {
    return {};
  }
}

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 0};
  }

  closed = false;
  async return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

const nonIterables = [
  new NotIterable(),
  new InvalidIterable(),
  undefined,
  null,
  0,
  false,
  Symbol(''),
  0n,
  {},
];

(async () => {
  for (const value of nonIterables) {
    const iter = new TestIterator();
    const mapped = iter.flatMap(x => value);

    assert.sameValue(iter.closed, false);
    try {
      await mapped.next();
      assert.sameValue(true, false, 'Expected reject');
    } catch (exc) {
      assert.sameValue(exc instanceof TypeError, true);
    }
    assert.sameValue(iter.closed, true);
  }
})();


reportCompare(0, 0);
