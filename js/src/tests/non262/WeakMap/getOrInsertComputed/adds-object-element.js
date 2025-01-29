// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Adds a value with an Object key if key is not already in the map.
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  8. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  9. Append p to M.[[WeakMapData]].
  ...
features: [WeakMap]
---*/

var map = new WeakMap();
var foo = {};
var bar = {};
var baz = {};

map.getOrInsertComputed(foo, () => 1);
map.getOrInsertComputed(bar, () => 2);
map.getOrInsertComputed(baz, () => 3);

assertEq(map.has(foo), true);
assertEq(map.has(bar), true);
assertEq(map.has(baz), true);

assertEq(map.get(foo), 1);
assertEq(map.get(bar), 2);
assertEq(map.get(baz), 3);

reportCompare(0, 0);
