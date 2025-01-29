// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsert)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Append a new value in the map normalizing +0 and -0.
info: |
  Map.prototype.getOrInsert ( key , value )

  ...
  3. Set key to CanonicalizeKeyedCollectionKey(key).
  4. For each Record { [[Key]], [[Value]] } p of M.[[MapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, return p.[[Value]].
  5. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  6. Append p to M.[[MapData]].
  ...
features: [Symbol]
---*/

var map = new Map();
map.getOrInsert(-0, 42);
assertEq(map.get(0), 42);

map = new Map();
map.getOrInsert(+0, 43);
assertEq(map.get(0), 43);

reportCompare(0, 0);
