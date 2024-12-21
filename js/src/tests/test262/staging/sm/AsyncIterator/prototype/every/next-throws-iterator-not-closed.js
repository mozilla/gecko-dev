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
    throw new Error();
  }

  closed = false;
  return() {
    this.closed = true;
  }
}

const fn = () => {};
const iter = new TestIterator();

assert.sameValue(iter.closed, false);
iter.every(fn).then(() => assert.sameValue(true, false, 'expected error'), err => {
  assert.sameValue(err instanceof Error, true);
  assert.sameValue(iter.closed, false);
});


reportCompare(0, 0);
