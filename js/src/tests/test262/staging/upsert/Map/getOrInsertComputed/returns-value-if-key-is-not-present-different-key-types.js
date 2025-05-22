// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Returns the value from the specified key on different types, when key not present.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  7. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  8. Append p to M.[[MapData]].
  9. Return p.[[Value]].
  ...
features: [Symbol, arrow-function, upsert]
flags: [noStrict]
---*/
var map = new Map();

assert.sameValue(map.getOrInsertComputed('bar', () => 0), 0);

assert.sameValue(map.getOrInsertComputed(1, () => 42), 42);

assert.sameValue(map.getOrInsertComputed(NaN, () => 1), 1);

var item = {};
assert.sameValue(map.getOrInsertComputed(item, () => 2), 2);

item = [];
assert.sameValue(map.getOrInsertComputed(item, () => 3), 3);

item = Symbol('item');
assert.sameValue(map.getOrInsertComputed(item, () => 4), 4);

item = null;
assert.sameValue(map.getOrInsertComputed(item, () => 5), 5);

item = undefined;
assert.sameValue(map.getOrInsertComputed(item, () => 6), 6);


reportCompare(0, 0);
