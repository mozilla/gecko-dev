// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Property type and descriptor.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  17 ECMAScript Standard Built-in Objects
  
includes: [propertyHelper.js]
features: [arrow-function]
---*/

assertEq(
  typeof Map.prototype.getOrInsertComputed,
  'function',
  '`typeof Map.prototype.getOrInsertComputed` is `function`'
);

assertDeepEq(Object.getOwnPropertyDescriptor(Map.prototype, "getOrInsertComputed"), {
  value: Map.prototype.getOrInsertComputed,
  writable: true,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
