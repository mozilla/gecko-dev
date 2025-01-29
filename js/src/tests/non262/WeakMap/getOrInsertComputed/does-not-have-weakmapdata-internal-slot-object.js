// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Throws TypeError if `this` doesn't have a [[WeakMapData]] internal slot.
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  2. Perform ? RequireInternalSlot(M, [[WeakMapData]]).
  ...
---*/

assertThrowsInstanceOf(function() {
  WeakMap.prototype.getOrInsertComputed.call({}, {}, () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  var map = new WeakMap();
  map.getOrInsertComputed.call({}, {}, () => 1);
}, TypeError);

reportCompare(0, 0);
