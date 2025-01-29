// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsert)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Returns the value given as parameter when key is not present.
info: |
  WeakMap.prototype.getOrInsert ( key, value )

  ...
  4. For each Record { [[Key]], [[Value]] } p of M.[[WeakMapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, return p.[[Value]].
  5. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  6. Append p to M.[[WeakMapData]].
  7. Return value.
features: [WeakMap]
---*/

var foo = {};
var bar = {};
var baz = [];
var map = new WeakMap();

assertEq(map.getOrInsert(foo, 0), 0);

assertEq(map.getOrInsert(bar, 1), 1);

assertEq(map.getOrInsert(baz, 2), 2);

reportCompare(0, 0);
