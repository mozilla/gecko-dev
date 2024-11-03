// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError1 extends Error {}
class CustomError2 extends Error {}

function createIterator(values, throwingNature) {
  return {
    [Symbol.asyncIterator]: () => ({
      i: 0,
      async return() {
        values.push("return");
        if (throwingNature === "return" || throwingNature === "both") {
          throw new CustomError2();
        }
        return { done: true };
      },
      async next() {
        return {
          value: {
            val: this.i++,
            async [Symbol.asyncDispose]() {
              values.push(this.val);
              if ((throwingNature === "disposal" || throwingNature === "both") && this.val === 3) {
                throw new CustomError1();
              }
            }
          },
          done: false
        }
      }
    })
  }
}

{
  const values = [];
  const iterator = createIterator(values, "disposal");

  async function testDisposalThrowsAreThrown() {
    for await (await using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOfAsync(testDisposalThrowsAreThrown, CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  const iterator = createIterator(values, "return");

  async function testReturnThrowsAreThrown() {
    for await (await using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOfAsync(testReturnThrowsAreThrown, CustomError2);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  const iterator = createIterator(values, "both");

  async function testReturnErrorsAreIgnoredIfDisposalThrows() {
    for await (await using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOfAsync(testReturnErrorsAreIgnoredIfDisposalThrows, CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  let values = [];

  async function testThrowsWithNonlocalJumpsWithLabels(iteratorThrowingNature) {
    const iter = createIterator(values, iteratorThrowingNature);
    outer: {
      for await (await using d of iter) {
        if (d.val === 3) {
          break outer;
        }
      }
    }
  }

  assertThrowsInstanceOfAsync(() => testThrowsWithNonlocalJumpsWithLabels("disposal"), CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);

  values = [];

  assertThrowsInstanceOfAsync(() => testThrowsWithNonlocalJumpsWithLabels("return"), CustomError2);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);

  values = [];
  assertThrowsInstanceOfAsync(() => testThrowsWithNonlocalJumpsWithLabels("both"), CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}
