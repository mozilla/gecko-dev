// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.flatMap closes the iterator when innerComplete throws.
info: |
  Iterator Helpers proposal 2.1.6.7 1. Repeat,
    ...
    k. Repeat, while innerAlive is true,
      ...
      v. Let innerComplete be IteratorComplete(innerNext).
      vi. IfAbruptCloseAsyncIterator(innerComplete, iterated).
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
  async next() {
    return {done: false, value: 0};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

class TestError extends Error {}
class InnerIterator extends AsyncIterator {
  async next() {
    return {
      get done() {
        throw new TestError();
      }
    };
  }
}

const iter = new TestIterator();
const mapped = iter.flatMap(x => new InnerIterator());

assert.sameValue(iter.closed, false);
mapped.next().then(
  _ => assert.sameValue(true, false, 'Expected reject.'),
  err => {
    assert.sameValue(err instanceof TestError, true);
    assert.sameValue(iter.closed, true);
  }
);


reportCompare(0, 0);
