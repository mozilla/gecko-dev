// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Append a new value as the last element of entries.
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

var s = Symbol(2);
var map = new Map([[4, 4], ['foo3', 3], [s, 2]]);

map.getOrInsertComputed(null, () => 42);
map.getOrInsertComputed(1, () => 'valid');

assertEq(map.size, 5);
assertEq(map.get(1), 'valid');

var results = [];

map.forEach(function(value, key) {
  results.push({
    value: value,
    key: key
  });
});

var result = results.pop();
assertEq(result.value, 'valid');
assertEq(result.key, 1);

result = results.pop();
assertEq(result.value, 42);
assertEq(result.key, null);

result = results.pop();
assertEq(result.value, 2);
assertEq(result.key, s);

reportCompare(0, 0);
