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
  async next() {
    return {done: true, value: 'value'};
  }
}

async function* gen(x) { yield x; }

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(1),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
  iter => iter.flatMap(gen),
];

(async () => {
  for (const method of methods) {
    const iterator = method(new TestIterator());
    const {done, value} = await iterator.next();
    assert.sameValue(done, true);
    assert.sameValue(value, undefined);
  }
})();


reportCompare(0, 0);
