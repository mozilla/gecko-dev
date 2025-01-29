// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsert)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Sune Eriksson Lianes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Inserts the value for the specified key on different types, when key not present.
info: |
  Map.prototype.getOrInsert ( key , value )

  ...
  5. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  6. Append p to M.[[MapData]].
  ...
feature: [Symbol]
---*/

var map = new Map();

map.getOrInsert('bar', 0);
assertEq(map.get('bar'), 0);

map.getOrInsert(1, 42);
assertEq(map.get(1), 42);

map.getOrInsert(NaN, 1);
assertEq(map.get(NaN), 1);

var item = {};
map.getOrInsert(item, 2);
assertEq(map.get(item), 2);

item = [];
map.getOrInsert(item, 3);
assertEq(map.get(item), 3);

item = Symbol('item');
map.getOrInsert(item, 4);
assertEq(map.get(item), 4);

item = null;
map.getOrInsert(item, 5);
assertEq(map.get(item), 5);

item = undefined;
map.getOrInsert(item, 6);
assertEq(map.get(item), 6);

reportCompare(0, 0);
