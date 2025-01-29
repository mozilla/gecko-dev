// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  If the callbackfn inserts a value on the given key, the value is overwritten.
info: |
  WeakMap.prototype.set ( key, value )

  ...
  6. Let value be ? Call(callbackfn, undefined, « key »).
  7. For each Record { [[Key]], [[Value]] } p of M.[[WeakMapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, then
      i. Set p.[[Value]] to value.
      ii. Return value.
  8. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  9. Append p to M.[[WeakMapData]].
  ...
---*/

var map = new WeakMap();
var foo = {};
var bar = {};
var baz = {};

map.getOrInsertComputed(foo, () => {map.set(foo, 0); return 3;});
map.getOrInsertComputed(bar, () => {map.set(bar, 1)});
map.getOrInsertComputed(baz, () => {map.set(baz, 2); return 'string';});

assertEq(map.get(foo), 3);
assertEq(map.get(bar), undefined);
assertEq(map.get(baz), 'string');

reportCompare(0, 0);
