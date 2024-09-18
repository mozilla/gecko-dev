// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  async function* gen() {
    yield 1;
    yield 2;
    yield 3;
  }

  const returned = [];
  const iter = gen();
  iter.return = function () {
    returned.push('return');
    throw new CustomError();
  }

  assertThrowsInstanceOfAsync(() => { return iter[Symbol.asyncDispose]() }, CustomError);
  assertArrayEq(returned, ['return']);
}

{
  async function* gen() {
    yield 1;
    yield 2;
    yield 3;
  }

  const returned = [];
  const iter = gen();
  iter.return = function () {
    returned.push('return');
    throw new CustomError();
  }

  async function testThrowInIteratorReturnRejectsWithAwaitUsingSyntax() {
    {
      returned.push((await iter.next()).value);
      await using it = iter;
    }
  }

  assertThrowsInstanceOfAsync(testThrowInIteratorReturnRejectsWithAwaitUsingSyntax, CustomError);
  assertArrayEq(returned, [1, 'return']);
}
