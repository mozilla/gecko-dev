// Test the telemetry added in Bug 1970930
assertEq(getUseCounterResults().ErrorCaptureStackTrace, 0);
assertEq(getUseCounterResults().ErrorCaptureStackTraceCtor, 0);
assertEq(getUseCounterResults().ErrorCaptureStackTraceUncallableCtor, 0);

Error.captureStackTrace({});
assertEq(getUseCounterResults().ErrorCaptureStackTrace, 1);
assertEq(getUseCounterResults().ErrorCaptureStackTraceCtor, 0);
assertEq(getUseCounterResults().ErrorCaptureStackTraceUncallableCtor, 0);

function MyError() {
  Error.captureStackTrace(this, MyError);
}
let myError = new MyError();

assertEq(getUseCounterResults().ErrorCaptureStackTrace, 2);
assertEq(getUseCounterResults().ErrorCaptureStackTraceCtor, 1);
assertEq(getUseCounterResults().ErrorCaptureStackTraceUncallableCtor, 0);

Error.captureStackTrace({}, false);
assertEq(getUseCounterResults().ErrorCaptureStackTrace, 3);
assertEq(getUseCounterResults().ErrorCaptureStackTraceCtor, 2);
assertEq(getUseCounterResults().ErrorCaptureStackTraceUncallableCtor, 1);
