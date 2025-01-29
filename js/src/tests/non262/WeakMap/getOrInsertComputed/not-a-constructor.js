// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  WeakMap.prototype.getOrInsertComputed does not implement [[Construct]], is not new-able
info: |
  ECMAScript Function Objects

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.

  sec-evaluatenew

  ...
  7. If IsConstructor(constructor) is false, throw a TypeError exception.
  ...
includes: [isConstructor.js]
features: [Reflect.construct, WeakMap, arrow-function]
---*/

assertEq(
  isConstructor(WeakMap.prototype.getOrInsertComputed),
  false,
  'isConstructor(WeakMap.prototype.getOrInsertComputed) must return false'
);

assertThrowsInstanceOf(() => {
  let wm = new WeakMap(); new wm.getOrInsertComputed({}, () => 1);
}, TypeError);

reportCompare(0, 0);
