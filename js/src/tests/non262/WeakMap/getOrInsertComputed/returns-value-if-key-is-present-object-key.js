// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Returns the value from the specified Object key
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  5. For each Record { [[Key]], [[Value]] } p of M.[[WeakMapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, return p.[[Value]].
  ...
features: [WeakMap]
---*/

var foo = {};
var bar = {};
var baz = [];
var map = new WeakMap([
  [foo, 0]
]);

assertEq(map.getOrInsertComputed(foo, () => 3), 0);

map.set(bar, 1);
assertEq(map.getOrInsertComputed(bar, () => 4), 1);

map.set(baz, 2);
assertEq(map.getOrInsertComputed(baz, () => 5), 2);

reportCompare(0, 0);
