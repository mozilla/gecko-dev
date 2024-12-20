// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  %AsyncIterator.prototype% methods ignore iterator mutation if already done.
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
  values = [1, 2];
  async next() {
    if (this.values.length == 0)
      return {done: true};
    return {done: false, value: this.values.shift()};
  }
}

function check({done, value}, expectedDone, expectedValue) {
  assert.sameValue(done, expectedDone);
  assert.sameValue(Array.isArray(value) ? value[1] : value, expectedValue);
}

const methods = [
  ['map', x => x],
  ['filter', x => true],
  ['take', Infinity],
  ['drop', 0],
  ['asIndexedPairs', undefined],
  ['flatMap', async function*(x) { yield x; }],
];

for (const [method, arg] of methods) {
  (async () => {
    const iter = new TestIterator();
    const iterHelper = iter[method](arg);
    check(await iterHelper.next(), false, 1);
    check(await iterHelper.next(), false, 2);
    check(await iterHelper.next(), true, undefined);
    iter.values.push(3);
    check(await iterHelper.next(), true, undefined);
  })();
}


reportCompare(0, 0);
