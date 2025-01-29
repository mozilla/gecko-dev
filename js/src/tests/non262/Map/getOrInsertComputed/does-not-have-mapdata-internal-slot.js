// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Sune Eriksson Lianes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
    Throws a TypeError if `this` object does not have a [[MapData]] internal slot.
info: |
    Map.getOrInsertComputed ( key , callbackfn )

    ...
  1. Let M be the this value.
  2. Perform ? RequireInternalSlot(M, [[MapData]]).
    ...

    features: [arrow-function]
---*/

var map = new Map();

assertThrowsInstanceOf(function () {
    Map.prototype.getOrInsertComputed.call([], 1, () => 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    map.getOrInsertComputed.call([], 1, () => 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    Map.prototype.getOrInsertComputed.call({}, 1, () => 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    map.getOrInsertComputed.call({}, 1, () => 1);
}, TypeError);

reportCompare(0, 0);
