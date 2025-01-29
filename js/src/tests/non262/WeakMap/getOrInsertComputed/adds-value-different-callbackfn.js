// |reftest| shell-option(--enable-upsert) skip-if(!WeakMap.prototype.getOrInsertComputed)
// Copyright (C) 2025 Jonas Haukenes, Mathias Ness. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: proposal-upsert
description: >
  Does not throw if `callbackfn` is callable.
info: |
  WeakMap.prototype.getOrInsertComputed ( key , callbackfn )

  ...
  3. If IsCallable(callbackfn) is false, throw a TypeError exception.
  ...
  features: [arrow-function]
---*/

var bar = {};
var baz = {};
var foo = {};
var foobar = {};
var foobarbaz = {};

var m = new WeakMap();

assertEq(
    m.getOrInsertComputed(bar, function() {return 1;})
    , 1);
assertEq(m.get(bar), 1);


assertEq(
    m.getOrInsertComputed(baz, () => 2)
    , 2);
assertEq(m.get(baz), 2);


function three() {return 3;}

assertEq(
    m.getOrInsertComputed(foo, three)
    , 3);
assertEq(m.get(foo), 3);


assertEq(
    m.getOrInsertComputed(foobar, new Function())
    , undefined);
assertEq(m.get(foobar), undefined);
 

assertEq(
    m.getOrInsertComputed(foobarbaz, (function() {return 5;}).bind(m))
    , 5);
assertEq(m.get(foobarbaz), 5);


reportCompare(0, 0);
