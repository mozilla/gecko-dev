// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Does not evaluate the callback function if the key is already in the map.
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  5. For each Record { [[Key]], [[Value]] } p of M.[[WeakMapData]], do
    a. If p.[[Key]] is not empty and SameValue(p.[[Key]], key) is true, return p.[[Value]].
  6. Let value be ? Call(callbackfn, undefined, « key »).
  ...
features: [WeakMap, upsert]
flags: [noStrict]
---*/
var map = new Map([
  [1, 0]
]);

function callback() {
    throw new Error('Callbackfn should not be evaluated if key is present');
}

assert.sameValue(map.getOrInsertComputed(1, callback), 0);

map.set(2, 1);
assert.sameValue(map.getOrInsertComputed(2, callback), 1);

map.set(3, 2);
assert.sameValue(map.getOrInsertComputed(3, callback), 2);

assert.throws(Error, function() {
  map.getOrInsertComputed(4, callback)
}, Error);


reportCompare(0, 0);
