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

class TestAsyncIterator extends AsyncIterator {
  async next(value) {
    return {done: false, value};
  }
}

const methods = [
  iter => iter.map(x => x),
  iter => iter.filter(x => true),
  iter => iter.take(2),
  iter => iter.drop(0),
  iter => iter.asIndexedPairs(),
];

for (const outerMethod of methods) {
  for (const innerMethod of methods) {
    const iterator = new TestAsyncIterator();
    const iteratorChain = outerMethod(innerMethod(iterator));
    iteratorChain.next().then(
      _ => iteratorChain.next('last value').then(
        ({done, value}) => {
          assert.sameValue(done, false);
          // Unwrap the asIndexedPair return values.
          while (Array.isArray(value)) {
            value = value[1];
          }
          assert.sameValue(value, 'last value');
        }
      ),
    );
  }
}


reportCompare(0, 0);
