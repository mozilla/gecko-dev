// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: Throws TypeError if key cannot be held weakly.
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  4. If CanBeHeldWeakly(_key_) is *false*, throw a *TypeError* exception.
  ...
features: [Symbol, WeakMap]
---*/

var s = new WeakMap();

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed(1, () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed(false, () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed(undefined, () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed('string', () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed(null, () => 1);
}, TypeError);

assertThrowsInstanceOf(function() {
  s.getOrInsertComputed(Symbol.for('registered symbol'), () => 1);
}, TypeError, 'Registered symbol not allowed as WeakMap key');

reportCompare(0, 0);
