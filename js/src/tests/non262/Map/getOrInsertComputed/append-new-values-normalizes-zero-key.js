// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Append a new value in the map normalizing +0 and -0.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  4. Set key to CanonicalizeKeyedCollectionKey(key).
  5. For each Record { [[Key]], [[Value]] } p of M.[[MapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, return p.[[Value]].
  6. Let value be ? Call(callbackfn, key).
  7. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  8. Append p to M.[[MapData]].
  ...
features: [Symbol, arrow-function]
---*/

var map = new Map();
map.getOrInsertComputed(-0, () => 42);
assertEq(map.get(0), 42);

map = new Map();
map.getOrInsertComputed(+0, () => 43);
assertEq(map.get(0), 43);

reportCompare(0, 0);
