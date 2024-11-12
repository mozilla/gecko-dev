// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError1 extends Error {}
class CustomError2 extends Error {}

function createIterator(values, throwingNature) {
  return {
    [Symbol.iterator]: () => ({
      i: 0,
      return() {
        values.push("return");
        if (throwingNature === "return" || throwingNature === "both") {
          throw new CustomError2();
        }
        return { done: true };
      },
      next() {
        return {
          value: {
            val: this.i++,
            [Symbol.dispose]() {
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

  function testDisposalThrowsAreThrown() {
    for (using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOf(testDisposalThrowsAreThrown, CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  const iterator = createIterator(values, "return");

  function testReturnThrowsAreThrown() {
    for (using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOf(testReturnThrowsAreThrown, CustomError2);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  const values = [];
  const iterator = createIterator(values, "both");

  function testReturnErrorsAreIgnoredIfDisposalThrows() {
    for (using d of iterator) {
      if (d.val === 3) {
        break;
      }
    }
  }

  assertThrowsInstanceOf(testReturnErrorsAreIgnoredIfDisposalThrows, CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}

{
  let values = [];

  function testThrowsWithNonlocalJumpsWithLabels(iteratorThrowingNature) {
    const iter = createIterator(values, iteratorThrowingNature);
    outer: {
      for (using d of iter) {
        if (d.val === 3) {
          break outer;
        }
      }
    }
  }

  assertThrowsInstanceOf(() => testThrowsWithNonlocalJumpsWithLabels("disposal"), CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);

  values = [];

  assertThrowsInstanceOf(() => testThrowsWithNonlocalJumpsWithLabels("return"), CustomError2);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);

  values = [];
  assertThrowsInstanceOf(() => testThrowsWithNonlocalJumpsWithLabels("both"), CustomError1);
  assertArrayEq(values, [0, 1, 2, 3, "return"]);
}
