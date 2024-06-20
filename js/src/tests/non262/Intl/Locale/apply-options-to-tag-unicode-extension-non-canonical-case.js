// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// "U" is not in canonical case.
let loc1 = new Intl.Locale("en-U-nu-latn", {numberingSystem: "thai"});
assertEq(loc1.toString(), "en-u-nu-thai");

// First letter of "Nu" is not in canonical case.
let loc2 = new Intl.Locale("en-u-Nu-latn", {numberingSystem: "thai"});
assertEq(loc2.toString(), "en-u-nu-thai");

// Second letter of "nU" is not in canonical case.
let loc3 = new Intl.Locale("en-u-nU-latn", {numberingSystem: "thai"});
assertEq(loc3.toString(), "en-u-nu-thai");

if (typeof reportCompare === "function")
  reportCompare(0, 0);
