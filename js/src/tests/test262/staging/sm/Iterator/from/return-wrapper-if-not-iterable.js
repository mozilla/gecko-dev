// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
  Iterator.from returns an iterator wrapper if O is not an iterable.

  Iterator is not enabled unconditionally
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- iterator-helpers
description: |
  pending
esid: pending
---*/
class TestIterator {
  next() {
    return { done: false, value: 0 };
  }
}

const iter = new TestIterator();
assert.sameValue(
  Symbol.iterator in iter,
  false,
  'iter is not an iterable.'
);

const wrapper = Iterator.from(iter);
assert.sameValue(iter !== wrapper, true);
assert.sameValue(
  Symbol.iterator in wrapper,
  true,
  'wrapper is an iterable.'
);


reportCompare(0, 0);
