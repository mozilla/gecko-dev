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

class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({ done: this.closed, value: undefined });
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const reducer = (x, y) => { throw new Error(); };
const iter = new TestIterator();

assert.sameValue(iter.closed, false);
iter.reduce(reducer).then(() => assert.sameValue(true, false, 'expected error'), err => {
  assert.sameValue(err instanceof Error, true);
  assert.sameValue(iter.closed, true);
});


reportCompare(0, 0);
