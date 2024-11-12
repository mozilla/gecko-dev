// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), 1, "2", null, undefined, { error: "test2" }, new CustomError("test3")];
  let e;
  async function testSuppressedErrorWithNoOutsideError() {
    await using x = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }
    await using y = {
      [Symbol.asyncDispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    }
    using z = {
      [Symbol.dispose]() {
        disposed.push(3);
        throw errorsToThrow[2];
      }
    }
    await using a = {
      [Symbol.dispose]() {
        disposed.push(4);
        throw errorsToThrow[3];
      }
    }
    await using b = {
      [Symbol.asyncDispose]: () => {
        disposed.push(5);
        throw errorsToThrow[4];
      }
    }
    await using c = {
      async [Symbol.asyncDispose]() {
        disposed.push(6);
        throw errorsToThrow[5];
      }
    }
    await using d = {
      [Symbol.asyncDispose]: async () => {
        disposed.push(7);
        throw errorsToThrow[6];
      }
    }
  }
  testSuppressedErrorWithNoOutsideError().catch((err) => { e = err });
  drainJobQueue();
  assertArrayEq(disposed, [7, 6, 5, 4, 3, 2, 1]);
  assertSuppressionChain(() => { throw e }, errorsToThrow);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), 1, new CustomError("test2")];
  let e;
  async function testSuppressedErrorWithOutsideError() {
    await using x = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }

    await using y = {
      [Symbol.asyncDispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    }

    throw errorsToThrow[2];
  }
  testSuppressedErrorWithOutsideError().catch((err) => { e = err });
  drainJobQueue();
  assertArrayEq(disposed, [2, 1]);
  assertSuppressionChain(() => { throw e }, errorsToThrow);
}

{
  const disposed = [];
  let e;
  async function testSuppressedErrorWithRuntimeErrors() {
    await using x = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
        undefined.x;
      }
    }
    await using y = {
      [Symbol.dispose]() {
        disposed.push(2);
        a.x;
      }
    }
    await using z = {
      [Symbol.asyncDispose]() {
        this[Symbol.asyncDispose]();
      }
    }

    null.x;
  }
  testSuppressedErrorWithRuntimeErrors().catch((err) => { e = err });
  drainJobQueue();
  assertArrayEq(disposed, [2, 1]);
  assertSuppressionChainErrorMessages(() => { throw e }, [
    {ctor: TypeError, message: "can't access property \"x\" of undefined"},
    {ctor: ReferenceError, message: "a is not defined"},
    {ctor: InternalError, message: "too much recursion"},
    {ctor: TypeError, message: "can't access property \"x\" of null"},
  ]);
}
