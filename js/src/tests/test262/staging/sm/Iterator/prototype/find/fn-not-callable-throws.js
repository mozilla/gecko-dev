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

assertThrowsInstanceOf(() => iter.find(), TypeError);
assertThrowsInstanceOf(() => iter.find(undefined), TypeError);
assertThrowsInstanceOf(() => iter.find(null), TypeError);
assertThrowsInstanceOf(() => iter.find(0), TypeError);
assertThrowsInstanceOf(() => iter.find(false), TypeError);
assertThrowsInstanceOf(() => iter.find(''), TypeError);
assertThrowsInstanceOf(() => iter.find(Symbol('')), TypeError);
assertThrowsInstanceOf(() => iter.find({}), TypeError);


reportCompare(0, 0);
