// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Calling `return` on a lazy %AsyncIterator.prototype% method closes the source iterator.
info: |
  Iterator Helpers proposal 2.1.6
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
    return {done: false, value: 1};
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
  const iter = new TestIterator();
  const iterHelper = method(iter);

  iterHelper.next().then(() => {
    assert.sameValue(iter.closed, false);
    iterHelper.return(0).then(({done, value}) => {
      assert.sameValue(iter.closed, true);
      assert.sameValue(done, true);
      assert.sameValue(value, 0);
    });
  });
}

for (const method of methods) {
  const iter = new TestIterator();
  const iterHelper = method(iter);

  assert.sameValue(iter.closed, false);
  iterHelper.return(0).then(({done, value}) => {
    assert.sameValue(iter.closed, true);
    assert.sameValue(done, true);
    assert.sameValue(value, 0);
  });
}


reportCompare(0, 0);
