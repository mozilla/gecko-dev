// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2024 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: > 
  Map.getOrInsertComputed throws when callbackfn throws, 
    return if abrubt completion Call(callbackfn, undefined, key)

info: |
  WeakMap.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  6. Let value be ? Call(callbackfn, undefined, key).
  ...
---*/

var map = new WeakMap();

var bar = {};

assertThrowsInstanceOf(function() {
  map.getOrInsertComputed(bar, function() {
    throw new Error('throw in callback');
  })
}, Error);

reportCompare(0, 0);
