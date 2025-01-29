// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Throws a TypeError if `callbackfn` is not callable.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  3. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...
features: [Symbol]
---*/

var m = new Map();

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, "");
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, true);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, undefined);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, null);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, {});
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, []);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, 1, Symbol());
}, TypeError);

reportCompare(0, 0);
