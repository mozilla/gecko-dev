// MakeDay: Adding finite |year| and |month| can result in non-finite intermediate result.

assertEq(Date.UTC(Number.MAX_VALUE, Number.MAX_VALUE), NaN);
assertEq(new Date(Number.MAX_VALUE, Number.MAX_VALUE).getTime(), NaN);

// https://github.com/tc39/ecma262/issues/1087

var d = new Date(0);
d.setUTCFullYear(Number.MAX_VALUE, Number.MAX_VALUE);
assertEq(d.getTime(), NaN);

if (typeof reportCompare === "function")
  reportCompare(true, true);
