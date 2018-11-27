// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.toString() returns this string value
es5id: 15.5.4.2_A1_T3
description: Create new String(string) and check its method toString()
---*/

var __string__obj = new String("metal");

//////////////////////////////////////////////////////////////////////////////
//CHECK#
if (__string__obj.toString() !== "metal") {
  $ERROR('#1: __string__obj = new String("metal"); __string__obj.toString() === "metal". Actual: __string__obj.toString() ===' + __string__obj.toString());
}
//
//////////////////////////////////////////////////////////////////////////////

reportCompare(0, 0);
