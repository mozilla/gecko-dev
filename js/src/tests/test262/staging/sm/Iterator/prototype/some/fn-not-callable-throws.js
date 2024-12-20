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

const iter = [].values();

assertThrowsInstanceOf(() => iter.some(), TypeError);
assertThrowsInstanceOf(() => iter.some(undefined), TypeError);
assertThrowsInstanceOf(() => iter.some(null), TypeError);
assertThrowsInstanceOf(() => iter.some(0), TypeError);
assertThrowsInstanceOf(() => iter.some(false), TypeError);
assertThrowsInstanceOf(() => iter.some(''), TypeError);
assertThrowsInstanceOf(() => iter.some(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.some({}), TypeError);


reportCompare(0, 0);
