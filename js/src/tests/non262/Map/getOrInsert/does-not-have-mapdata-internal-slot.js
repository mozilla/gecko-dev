// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsert)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Sune Eriksson Lianes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
    Throws a TypeError if `this` object does not have a [[MapData]] internal slot.
info: |
    Map.getOrInsert ( key , value )

    ...
    1. Let M be the this value.
    2. Perform ? RequireInternalSLot(M, [[MapData]])
    ...
---*/

var map = new Map();

assertThrowsInstanceOf(function () {
    Map.prototype.getOrInsert.call([], 1, 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    map.getOrInsert.call([], 1, 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    Map.prototype.getOrInsert.call({}, 1, 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    map.getOrInsert.call({}, 1, 1);
}, TypeError);

reportCompare(0, 0);
