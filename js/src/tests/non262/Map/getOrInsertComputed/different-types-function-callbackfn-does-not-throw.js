// |reftest| shell-option(--enable-upsert) skip-if(!Map.prototype.getOrInsertComputed)
// Copyright (C) 2024 Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Does not throw if `callbackfn` is callable.
info: |
  Map.prototype.getOrInsertComputed ( key , callbackfn )

   ...
  3. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...

  features: [arrow-function]
---*/

var m = new Map();


assertEq(
    m.getOrInsertComputed(1, function() {return 1;})
    , 1);
assertEq(m.get(1), 1);


assertEq(
    m.getOrInsertComputed(2, () => 2)
    , 2);
assertEq(m.get(2), 2);


function three() {return 3;}

assertEq(
    m.getOrInsertComputed(3, three)
    , 3);
assertEq(m.get(3), 3);


assertEq(
    m.getOrInsertComputed(4, new Function())
    , undefined);
assertEq(m.get(4), undefined);
 

assertEq(
    m.getOrInsertComputed(5, (function() {return 5;}).bind(m))
    , 5);
assertEq(m.get(5), 5);


reportCompare(0, 0);
