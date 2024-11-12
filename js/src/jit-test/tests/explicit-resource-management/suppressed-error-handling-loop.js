// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function testDisposedWithThrowInOrdinaryLoop() {
    const disposables = [
      {
        [Symbol.dispose]() {
          disposed.push(1);
          throw errorsToThrow[0];
        }
      }
    ];
    for (let i = 0; i < 5; i++) {
      using x = disposables[i];
      throw errorsToThrow[1];
    }
  }
  assertSuppressionChain(testDisposedWithThrowInOrdinaryLoop, errorsToThrow);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function testDisposedWithThrowInLoopRequiringIteratorClose() {
    const disposables = [
      {
        [Symbol.dispose]() {
          disposed.push(1);
          throw errorsToThrow[0];
        }
      }
    ]
    for (const d of disposables) {
      using x = d;
      throw errorsToThrow[1];
    }
  }
  assertSuppressionChain(testDisposedWithThrowInLoopRequiringIteratorClose, errorsToThrow);
  assertArrayEq(disposed, [1]);
}

{
  const values = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function testDisposedWithThrowInLoopWithCustomIterable() {
    const disposables = [
      {
        val: "a",
        [Symbol.dispose]() {
          values.push(this.val);
        }
      },
      {
        val: "b",
        [Symbol.dispose]() {
          values.push(this.val);
          throw errorsToThrow[0];
        }
      },
    ];
    const iterable = {
      [Symbol.iterator]() {
        let i = 0;
        return {
          next() {
            if (i === disposables.length) {
              return { done: true };
            }
            return { value: disposables[i++], done: false };
          },
          return() {
            values.push("return");
            return { done: true };
          }
        }
      }
    }
    for (using d of iterable) {
      if (d.val === "b") {
        throw errorsToThrow[1];
      }
    }
  }
  assertSuppressionChain(testDisposedWithThrowInLoopWithCustomIterable, errorsToThrow);
  assertArrayEq(values, ["a", "b", "return"]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2"), new Error("test3"), new Error("test4")];
  function testDisposeWithThrowInForOfLoop() {
    const d1 = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    }
    const d2 = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    }

    for (using d of [d1, d2]) {
      throw errorsToThrow[2];
    }
  }
  assertSuppressionChain(testDisposeWithThrowInForOfLoop, [errorsToThrow[0], errorsToThrow[2]]);
  assertArrayEq(disposed, [1]);
}
