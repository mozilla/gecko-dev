// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes, Sune Eriksson Lianes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Throws a TypeError if `this` is not an Object.
info: |
  WeakMap.prototype.getOrInsertComputed ( key , callbackfn )

  1. Let M be the this value
  2. Perform ? RequireInternalSlot(M, [[WeakMapData]])
  ...
features: [Symbol, upsert]
flags: [noStrict]
---*/
var m = new WeakMap();

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(false, {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(1, {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call("", {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(undefined, {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(null, {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(Symbol(), {}, () => 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call('', {}, () => 1);
});


reportCompare(0, 0);
