// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed||!xulRuntime.shell) -- upsert is not enabled unconditionally, requires shell-options
// Copyright (C) 2015 the V8 project authors. All rights reserved.
// Copyright (C) 2025 Jonas Haukenes. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: |
  Returns the value from the specified Object key
info: |
  WeakMap.prototype.getOrInsertComputed ( key, callbackfn )

  ...
  8. Let p be the Record { [[Key]]: key, [[Value]]: value }.
  9. Append p to M.[[WeakMapData]].
  10. Return value.
features: [WeakMap, upsert]
flags: [noStrict]
---*/
var foo = {};
var bar = {};
var baz = [];
var map = new WeakMap();

assert.sameValue(map.getOrInsertComputed(foo, () => 0), 0);

assert.sameValue(map.getOrInsertComputed(bar, () => 1), 1);

assert.sameValue(map.getOrInsertComputed(baz, () => 2), 2);


reportCompare(0, 0);
