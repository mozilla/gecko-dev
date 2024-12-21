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

class TestError extends Error {}

class TestIterator extends AsyncIterator {
  async next() {
    return {done: false, value: 'value'};
  }

  closed = false;
  async return(value) {
    this.closed = true;
    return {done: true, value};
  }
}

function checkIterResult({done, value}, expectedDone, expectedValue) {
  assert.sameValue(done, expectedDone);
  assert.sameValue(Array.isArray(value) ? value[1] : value, expectedValue);
}

const otherGlobal = createNewGlobal({newCompartment: true});

const methods = [
  ["map", x => x],
  ["filter", x => true],
  ["take", Infinity],
  ["drop", 0],
  ["asIndexedPairs", undefined],
  ["flatMap", async function*(x) { yield x; }],
];

const {next: otherNext, return: otherReturn, throw: otherThrow} =
  Object.getPrototypeOf(otherGlobal.eval("(async function*() {})().map(x => x)"));

(async () => {
  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    checkIterResult(await otherNext.call(helper), false, 'value');
  }

  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    assert.sameValue(iterator.closed, false);
    checkIterResult(await otherReturn.call(helper), true, undefined);
    assert.sameValue(iterator.closed, true);
  }

  for (const [method, arg] of methods) {
    const iterator = new TestIterator();
    const helper = iterator[method](arg);
    try {
      await otherThrow.call(helper, new TestError());
      assert.sameValue(true, false, 'Expected exception');
    } catch (exc) {
      assert.sameValue(exc instanceof TestError, true);
    }
    checkIterResult(await helper.next(), true, undefined);
  }
})();


reportCompare(0, 0);
