/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

// bug 953111

var A = TypedObject.uint8.array(0);
var a = new A();
a.forEach(function(val, i) {});

// bug 951356 (dup, but a dup that is more likely to crash)

var AA = TypedObject.uint8.array(2147483647).array(0);
var aa = new AA();
