// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  WeakMap.prototype.getOrInsertComputed.name descriptor
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assertDeepEq(Object.getOwnPropertyDescriptor(Map.prototype.getOrInsertComputed, "name"), {
  value: "getOrInsertComputed",
  writable: false,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
