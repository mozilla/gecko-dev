// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_4-1-1
description: JSON.stringify of a circular object throws a TypeError
---*/

var obj = {};
obj.prop = obj;

assert.throws(TypeError, function() {
  JSON.stringify(obj);
});

reportCompare(0, 0);
