// |reftest| shell-option(--enable-iterator-helpers) skip-if(!this.hasOwnProperty('Iterator')||!xulRuntime.shell) -- iterator-helpers is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Throw TypeError if `next` call returns non-object.
features:
- iterator-helpers
includes: [sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
---*/
//

const iterator = returnValue => Object.setPrototypeOf({
  next: () => returnValue,
}, Iterator.prototype);
const mapper = x => x;

assertThrowsInstanceOf(() => iterator(undefined).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(null).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(0).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(false).map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator('').map(mapper).next(), TypeError);
assertThrowsInstanceOf(() => iterator(Symbol()).map(mapper).next(), TypeError);


reportCompare(0, 0);
