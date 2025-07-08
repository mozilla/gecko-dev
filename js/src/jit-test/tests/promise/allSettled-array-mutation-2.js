let called = 0;
let fulfillFunction;
let rejectFunction;
class CustomPromise {
  constructor(executor) {
    executor(changeArray, changeArray);
  }
  static resolve() {
    return {
      then: (fulfill, reject) => {
        fulfillFunction = fulfill;
        rejectFunction = reject;
      }
    };
  }
};
function changeArray(result) {
  called++;
  assertEq(result.length, 1);
  result[0] = undefined;
}
Promise.allSettled.call(CustomPromise, [0]);
rejectFunction();
fulfillFunction();
assertEq(called, 1);
