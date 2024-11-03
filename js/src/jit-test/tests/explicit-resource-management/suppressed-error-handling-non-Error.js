// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const throwObject = { message: 'Hello world' };
  function testNonErrorObjectThrowsDuringDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw 1;
      }
    }
    using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw "test";
      }
    }
    using z = {
      [Symbol.dispose]() {
        throw null;
      }
    }

    throw throwObject;
  }

  assertSuppressionChain(testNonErrorObjectThrowsDuringDispose, [
    1, "test", null, throwObject
  ]);
  assertArrayEq(disposed, [2, 1]);
}

{
  const disposed = [];
  class SubError extends Error {
    constructor(num) {
      super();
      this.name = 'SubError';
      this.ident = num;
    }
  }
  const errorsToThrow = [new SubError(1), new SubError(2)];
  function testCustomErrorObjectThrowsDuringDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }
    using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    }
  }
  assertSuppressionChain(testCustomErrorObjectThrowsDuringDispose, errorsToThrow);
  assertArrayEq(disposed, [2, 1]);
}
