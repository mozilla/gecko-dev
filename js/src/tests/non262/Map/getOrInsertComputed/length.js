// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Map.prototype.getOrInsert.length value and descriptor.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assertDeepEq(Object.getOwnPropertyDescriptor(Map.prototype.getOrInsertComputed, "length"), {
  value: 2,
  writable: false,
  enumerable: false,
  configurable: true
});

reportCompare(0, 0);
