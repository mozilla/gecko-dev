// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  async function testAwaitUsingInFunction() {
    await using x = {
      [Symbol.asyncDispose]() {
        disposed.push(1);
      }
    }

    await using y = {
      [Symbol.asyncDispose]() {
        disposed.push(2);
      }
    }

    await using z = {
      [Symbol.asyncDispose]() {
        disposed.push(3);
      }
    }
  }
  testAwaitUsingInFunction();
  // An async function is synchronously executed until the first await.
  // herein at the end of scope the await using inserts the first await
  // thus before awaiting this function we only see 3 to be pushed
  // into the disposed array.
  assertArrayEq(disposed, [3]);
  drainJobQueue();
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const values = [];
  async function testDisposedInFunctionAndBlock() {
    await using a = {
      [Symbol.dispose]: () => values.push("a")
    };

    await using b = {
      [Symbol.dispose]: () => values.push("b")
    };

    {
      await using c = {
        [Symbol.dispose]: () => values.push("c")
      };
  
      {
        await using d = {
          [Symbol.dispose]: () => values.push("d")
        };
      }

      await using e = {
        [Symbol.dispose]: () => values.push("e")
      };
    }

    await using f = {
      [Symbol.dispose]: () => values.push("f")
    };

    values.push("g");
  }
  testDisposedInFunctionAndBlock();
  assertArrayEq(values, ["d"]);
  drainJobQueue();
  assertArrayEq(values, ["d", "e", "c", "g", "f", "b", "a"]);
}
