// Test the telemetry added in Bug 1942684
assertEq(getUseCounterResults().ErrorStackGetter, 0);
assertEq(getUseCounterResults().ErrorStackGetterNoErrorData, 0);
assertEq(getUseCounterResults().ErrorStackSetter, 0);
assertEq(getUseCounterResults().ErrorStackSetterNonString, 0);
assertEq(getUseCounterResults().ErrorStackSetterNoErrorData, 0);

var e = new Error();
e.stack;
assertEq(getUseCounterResults().ErrorStackGetter, 1);
e.stack = "hi";
assertEq(getUseCounterResults().ErrorStackSetter, 1);

var e2 = new Error();
e2.stack = 42;
assertEq(getUseCounterResults().ErrorStackSetter, 2);
assertEq(getUseCounterResults().ErrorStackSetterNonString, 1);

var obj = Object.create(Error.prototype);
obj.stack;
assertEq(getUseCounterResults().ErrorStackGetter, 2);
assertEq(getUseCounterResults().ErrorStackGetterNoErrorData, 1);
obj.stack = "hi";
assertEq(getUseCounterResults().ErrorStackSetter, 3);
assertEq(getUseCounterResults().ErrorStackSetterNoErrorData, 1);

