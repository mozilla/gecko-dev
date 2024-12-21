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

const generatorProto = Object.getPrototypeOf(
  Object.getPrototypeOf(
    (function *() {})()
  )
);

const iteratorHelper = [0].values().map(x => x);

assertThrowsInstanceOf(() => generatorProto.next.call(iteratorHelper), TypeError);
assertThrowsInstanceOf(() => generatorProto.return.call(iteratorHelper), TypeError);
assertThrowsInstanceOf(() => generatorProto.throw.call(iteratorHelper), TypeError);


reportCompare(0, 0);
