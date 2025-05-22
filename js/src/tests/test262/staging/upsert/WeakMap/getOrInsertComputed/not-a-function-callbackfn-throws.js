// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Throws a TypeError if `callbackfn` is not callable.
info: |
  WeakMap.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  3. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...
features: [Symbol, upsert]
flags: [noStrict]
---*/
var bar = {};
var m = new WeakMap();

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, 1);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, "");
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, true);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, undefined);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, null);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, {});
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, []);
});

assert.throws(TypeError, function () {
    m.getOrInsertComputed.call(m, bar, Symbol());
});


reportCompare(0, 0);
