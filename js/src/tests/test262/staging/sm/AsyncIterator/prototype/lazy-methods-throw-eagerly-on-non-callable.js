// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %AsyncIterator.prototype% methods throw eagerly when passed non-callables.
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
async function* gen() {}

const methods = [
  (iter, fn) => iter.map(fn),
  (iter, fn) => iter.filter(fn),
  (iter, fn) => iter.flatMap(fn),
];

for (const method of methods) {
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, 0), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, false), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, undefined), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, null), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, ''), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(AsyncIterator.prototype, {}), TypeError);

  assertThrowsInstanceOf(() => method(gen(), 0), TypeError);
  assertThrowsInstanceOf(() => method(gen(), false), TypeError);
  assertThrowsInstanceOf(() => method(gen(), undefined), TypeError);
  assertThrowsInstanceOf(() => method(gen(), null), TypeError);
  assertThrowsInstanceOf(() => method(gen(), ''), TypeError);
  assertThrowsInstanceOf(() => method(gen(), Symbol('')), TypeError);
  assertThrowsInstanceOf(() => method(gen(), {}), TypeError);
}


reportCompare(0, 0);
