// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 939715;
var summary = 'method instance.filter';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var uint16 = TypedObject.uint16;
var uint32 = TypedObject.uint32;
var uint8Clamped = TypedObject.uint8Clamped;
var int8 = TypedObject.int8;
var int16 = TypedObject.int16;
var int32 = TypedObject.int32;
var float32 = TypedObject.float32;
var float64 = TypedObject.float64;

var objectType = TypedObject.objectType;

function filterOdds() {
  var length = 100;
  var Uint32s = new ArrayType(uint32, 100);
  var uint32s = new Uint32s();
  for (var i = 0; i < length; i++)
    uint32s[i] = i;

  var odds = uint32s.filter(i => (i % 2) != 0);
  assertEq(50, odds.length);
  for (var i = 0, j = 1; j < length; i++, j += 2)
    assertEq(odds[i], j);
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    filterOdds();

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
