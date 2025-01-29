// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Throws a TypeError if `callbackfn` is not callable.
info: |
  WeakMap.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  3. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...
features: [Symbol]
---*/

var bar = {};
var m = new WeakMap();

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, 1);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, "");
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, true);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, undefined);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, null);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, {});
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, []);
}, TypeError);

assertThrowsInstanceOf(function () {
    m.getOrInsertComputed.call(m, bar, Symbol());
}, TypeError);

reportCompare(0, 0);
