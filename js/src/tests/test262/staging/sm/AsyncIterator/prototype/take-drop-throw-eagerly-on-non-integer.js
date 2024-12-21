// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  `take` and `drop` throw eagerly when passed values that can't be converted to numbers.
info: |
  Iterator Helpers proposal 2.1.6.4 and 2.1.6.5
features:
- async-iteration
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/

//
//
async function* gen() {}
const iter = gen();
const methods = [
  value => iter.take(value),
  value => iter.drop(value),
];

const objectWithToPrimitive = {
  [Symbol.toPrimitive]() {
    return {};
  }
};

for (const method of methods) {
  assertThrowsInstanceOf(() => method(0n), TypeError);
  assertThrowsInstanceOf(() => method(Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(objectWithToPrimitive), TypeError);
}


reportCompare(0, 0);
