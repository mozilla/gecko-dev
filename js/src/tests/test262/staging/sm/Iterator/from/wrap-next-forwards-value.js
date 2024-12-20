// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- iterator-helpers
info: |
  Iterator is not enabled unconditionally
description: |
  pending
esid: pending
---*/
class Iter {
  next(value) {
    assert.sameValue(arguments.length, 0);
    return { done: false, value };
  }
}

const iter = new Iter();
const wrap = Iterator.from(iter);
assert.sameValue(iter !== wrap, true);

assert.sameValue(iter.v, undefined);
wrap.next(1);


reportCompare(0, 0);
