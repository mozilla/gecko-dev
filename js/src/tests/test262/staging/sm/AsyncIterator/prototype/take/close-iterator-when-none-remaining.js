// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype%.take closes the iterator when remaining is 0.
info: |
  Iterator Helpers proposal 2.1.6.4
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
    return {done: false, value: 'value'};
  }

  closed = false;
  async return() {
    this.closed = true;
    return {done: true};
  }
}

const iter = new TestIterator();
const iterTake = iter.take(1);

iterTake.next().then(
  ({done, value}) => {
    assert.sameValue(done, false);
    assert.sameValue(value, 'value');
    assert.sameValue(iter.closed, false);

    iterTake.next().then(
      ({done, value}) => {
        assert.sameValue(done, true);
        assert.sameValue(value, undefined);
        assert.sameValue(iter.closed, true);
      }
    );
  }
);


reportCompare(0, 0);
