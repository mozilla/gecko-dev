// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const values = [];
  async function* gen() {
    yield 1;
    await using a = {
      [Symbol.asyncDispose]() {
        values.push("a");
      }
    }
    yield 2;
    await using b = {
      [Symbol.asyncDispose]() {
        values.push("b");
      }
    }
    throw new CustomError();
    yield 3;
  }
  let caught = false;
  async function testDisposalDuringThrowInGenerator() {
    let x = gen();
    values.push((await x.next()).value);
    values.push((await x.next()).value);
    try {
      await x.next();
    } catch (e) {
      assertEq(e instanceof CustomError, true);
      caught = true;
    }
  }
  testDisposalDuringThrowInGenerator();
  drainJobQueue();
  assertEq(caught, true);
  assertArrayEq(values, [1, 2, "b", "a"]);
}

{
  const values = [];
  async function* gen() {
    yield 1;
    await using a = {
      [Symbol.asyncDispose]() {
        values.push("a");
      }
    }
    yield 2;
    await using b = {
      [Symbol.asyncDispose]() {
        values.push("b");
      }
    }
    yield 3;
  }
  let caught = false;
  async function testDisposalDuringForcedThrowInGenerator() {
    let x = gen();
    values.push((await x.next()).value);
    values.push((await x.next()).value);
    try {
      // the generator was resumed right at the point where
      // we have yield 2, and the throw statement is inserted right over
      // there preventing the evaluation of `await using b`.
      await x.throw(new CustomError());
    } catch (e) {
      assertEq(e instanceof CustomError, true);
      caught = true;
    }
  }
  testDisposalDuringForcedThrowInGenerator();
  drainJobQueue();
  assertEq(caught, true);
  assertArrayEq(values, [1, 2, "a"]);
}
