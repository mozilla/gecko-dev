// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.valueOf() returns this string value
es5id: 15.5.4.3_A1_T2
description: Create String object as new String(true) and check its valueOf()
---*/

var __string__obj = new String(true);

//////////////////////////////////////////////////////////////////////////////
//CHECK#
if (__string__obj.valueOf() !== "" + true) {
  $ERROR('#1: __string__obj = new String(true); __string__obj.valueOf() === ""+true. Actual: __string__obj.valueOf() ===' + __string__obj.valueOf());
}
//
//////////////////////////////////////////////////////////////////////////////

reportCompare(0, 0);
