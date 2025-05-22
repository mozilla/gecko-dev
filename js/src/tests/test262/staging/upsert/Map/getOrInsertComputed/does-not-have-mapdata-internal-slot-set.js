// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2024 Sune Eriksson Lianes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Throws a TypeError if `this` is a Set Object
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  1. Let M be the this value.
  2. Perform ? RequireInternalSlot(M, [[MapData]])
  ...
features: [Set, arrow-function, upsert]
flags: [noStrict]
---*/
assert.throws(TypeError, function () {
  Map.prototype.getOrInsertComputed.call(new Set(), 1, () => 1);
});

assert.throws(TypeError, function () {
  var map = new Map();
  map.getOrInsertComputed.call(new Set(), 1, () => 1);
});


reportCompare(0, 0);
