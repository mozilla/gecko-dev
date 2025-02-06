// Test the telemetry added in Bug 1944621
assertEq(getUseCounterResults().DateParse, 0);
assertEq(getUseCounterResults().DateParseImplDef, 0);

Date.parse("2025-01-29");
assertEq(getUseCounterResults().DateParse, 1);
assertEq(getUseCounterResults().DateParseImplDef, 0);

Date.parse("23Apr0024");
assertEq(getUseCounterResults().DateParse, 2);
assertEq(getUseCounterResults().DateParseImplDef, 1);

var d = new Date("2025-01-29");
assertEq(getUseCounterResults().DateParse, 3);
assertEq(getUseCounterResults().DateParseImplDef, 1);

var d = new Date("23Apr0024");
assertEq(getUseCounterResults().DateParse, 4);
assertEq(getUseCounterResults().DateParseImplDef, 2);

