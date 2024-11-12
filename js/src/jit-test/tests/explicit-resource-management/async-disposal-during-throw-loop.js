// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const disposed = [];
  let catchCalled = false;
  async function testAsyncDisposalInLoopDuringThrow() {
    const disposables = [
      {
        val: "a",
        [Symbol.asyncDispose]() {
          disposed.push(this.val);
        }
      },
      {
        val: "b",
        [Symbol.asyncDispose]() {
          disposed.push(this.val);
        }
      },
      {
        val: "c",
        [Symbol.asyncDispose]() {
          disposed.push(this.val);
        }
      }
    ];
    for (await using d of disposables) {
      if (d.val === "b") {
        throw new CustomError();
      }
    }
  }
  testAsyncDisposalInLoopDuringThrow().catch((e) => {
    catchCalled = true;
    assertEq(e instanceof CustomError, true);
  });
  drainJobQueue();
  assertEq(catchCalled, true);
  assertArrayEq(disposed, ["a", "b"]);
}

{
  const disposed = [];
  let catchCalled = false;
  async function testDisposalInLoopWithIteratorClose() {
    const disposables = [
      {
        val: "a",
        [Symbol.dispose]() {
          disposed.push(this.val);
        }
      },
      {
        val: "b",
        [Symbol.dispose]() {
          disposed.push(this.val);
        }
      },
      {
        val: "c",
        [Symbol.dispose]() {
          disposed.push(this.val);
        }
      }
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
            disposed.push("return");
            return { done: true };
          }
        }
      }
    }
    for (await using d of iterable) {
      if (d.val === "b") {
        throw new CustomError();
      }
    }
  }
  testDisposalInLoopWithIteratorClose().catch((e) => {
    catchCalled = true;
    assertEq(e instanceof CustomError, true);
  });
  drainJobQueue();
  assertEq(catchCalled, true);
  assertArrayEq(disposed, ["a", "b", "return"]);
}
