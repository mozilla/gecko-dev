// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  function testUndefinedAccessSuppressedErrorWithThrowInDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        undefined.x;
      }
    }
  }
  assertSuppressionChainErrorMessages(testUndefinedAccessSuppressedErrorWithThrowInDispose, [{ctor: TypeError, message: "can't access property \"x\" of undefined"}]);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  function testReferenceErrorSuppressedErrorWithThrowInDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        y.x;
      }
    }
  }
  assertSuppressionChainErrorMessages(testReferenceErrorSuppressedErrorWithThrowInDispose, [{ctor: ReferenceError, message: "y is not defined"}]);
}

{
  const disposed = [];
  function testMultipleRuntimeErrorsWithThrowsDuringDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        undefined.x;
      }
    }
    using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        a.x;
      }
    }
    using z = {
      [Symbol.dispose]() {
        this[Symbol.dispose]();
      }
    }
  }
  assertSuppressionChainErrorMessages(testMultipleRuntimeErrorsWithThrowsDuringDispose, [
    {ctor: TypeError, message: "can't access property \"x\" of undefined"},
    {ctor: ReferenceError, message: "a is not defined"},
    {ctor: InternalError, message: "too much recursion"},
  ]);
}

{
  const disposed = [];
  function testMultipleRuntimeErrorsWithThrowsDuringDisposeAndOutsideDispose() {
    using x = {
      [Symbol.dispose]() {
        disposed.push(1);
        undefined.x;
      }
    }
    using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        a.x;
      }
    }
    using z = {
      [Symbol.dispose]() {
        this[Symbol.dispose]();
      }
    }

    null.x;
  }
  assertSuppressionChainErrorMessages(testMultipleRuntimeErrorsWithThrowsDuringDisposeAndOutsideDispose, [
    {ctor: TypeError, message: "can't access property \"x\" of undefined"},
    {ctor: ReferenceError, message: "a is not defined"},
    {ctor: InternalError, message: "too much recursion"},
    {ctor: TypeError, message: "can't access property \"x\" of null"},
  ]);
}

{
  const values = [];
  function* gen() {
    using d = {
      value: "d",
      [Symbol.dispose]() {
        values.push(this.value);
      }
    }
    yield "a";
    yield "b";
    using c = {
      value: "c",
      [Symbol.dispose]() {
        values.push(this.value);
        a.x;
      }
    }
    null.x;
  }
  function testRuntimeErrorsWithGenerators() {
    let x = gen();
    values.push(x.next().value);
    values.push(x.next().value);
    x.next();
  }
  assertSuppressionChainErrorMessages(testRuntimeErrorsWithGenerators, [
    {ctor: ReferenceError, message: "a is not defined"},
    {ctor: TypeError, message: "can't access property \"x\" of null"}
  ]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
      null.x;
    }
  }
  function testRuntimeErrorWithLoops() {
    for (using x of [d]) {
      y.a;
    }
  }
  assertSuppressionChainErrorMessages(testRuntimeErrorWithLoops, [
    {ctor: TypeError, message: "can't access property \"x\" of null"},
    {ctor: ReferenceError, message: "y is not defined"},
  ]);
}
