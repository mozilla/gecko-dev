// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Sune Eriksson Lianes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Inserts the value for the specified key on different types, when key not present.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  7. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  8. Append p to M.[[MapData]].
  ...
feature: [Symbol, arrow-function]
---*/

var map = new Map();

map.getOrInsertComputed('bar', () => 0);
assertEq(map.get('bar'), 0);

map.getOrInsertComputed(1, () => 42);
assertEq(map.get(1), 42);

map.getOrInsertComputed(NaN, () => 1);
assertEq(map.get(NaN), 1);

var item = {};
map.getOrInsertComputed(item, () => 2);
assertEq(map.get(item), 2);

item = [];
map.getOrInsertComputed(item, () => 3);
assertEq(map.get(item), 3);

item = Symbol('item');
map.getOrInsertComputed(item, () => 4);
assertEq(map.get(item), 4);

item = null;
map.getOrInsertComputed(item, () => 5);
assertEq(map.get(item), 5);

item = undefined;
map.getOrInsertComputed(item, () => 6);
assertEq(map.get(item), 6);

reportCompare(0, 0);
