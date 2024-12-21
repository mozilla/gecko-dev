// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %AsyncIterator.prototype% methods don't close the iterator if `next` returns rejected promise.
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
class TestIterator extends AsyncIterator {
  next() {
    return Promise.reject('rejection');
  }

  closed = false;
  async return(value) {
    this.closed = true;
    return {done: true, value};
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
  const iterator = new TestIterator();
  assert.sameValue(iterator.closed, false);
  method(iterator).next().then(
    _ => assert.sameValue(true, false, 'Expected reject'),
    err => {
      assert.sameValue(err, 'rejection');
      assert.sameValue(iterator.closed, false);
    },
  );
}


reportCompare(0, 0);
