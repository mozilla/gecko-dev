// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- iterator-helpers
description: |
  pending
esid: pending
---*/

const methods = [
  iter => iter.map,
  iter => iter.filter,
  iter => iter.flatMap,
];

for (const method of methods) {
  const iter = [1].values();
  const iterMethod = method(iter);
  let iterHelper;
  iterHelper = iterMethod.call(iter, x => iterHelper.next());
  assertThrowsInstanceOf(() => iterHelper.next(), TypeError);
}


reportCompare(0, 0);
