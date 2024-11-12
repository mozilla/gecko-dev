// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const errorsToThrow = [undefined, null, 2];
  function testThrowsWithUndefinedAndNullValues() {
    using a = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }

    using b = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    }

    using c = {
      [Symbol.dispose]() {
        disposed.push(3);
        throw errorsToThrow[2];
      }
    }
  }
  assertSuppressionChain(testThrowsWithUndefinedAndNullValues, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const disposed = [];
  const errorsToThrow = [2, undefined];
  function testThrowsWithUndefinedAndRealValue() {
    using a = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }

    throw errorsToThrow[1];
  }
  assertSuppressionChain(testThrowsWithUndefinedAndRealValue, errorsToThrow);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const errorsToThrow = [2, null];
  function testThrowsWithNullAndRealValue() {
    using a = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }

    throw errorsToThrow[1];
  }
  assertSuppressionChain(testThrowsWithNullAndRealValue, errorsToThrow);
  assertArrayEq(disposed, [1]);
}
