// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposedInForOf = [];
  async function testDisposalsInForOf() {
    const disposables = [
      {
        value: "a",
        [Symbol.asyncDispose]: () => disposedInForOf.push("disposed a")
      },
      {
        value: "b",
        [Symbol.asyncDispose]: () => disposedInForOf.push("disposed b")
      },
      {
        value: "c",
        [Symbol.asyncDispose]: () => disposedInForOf.push("disposed c")
      }
    ];
    for (await using disposable of disposables) {
      disposedInForOf.push(disposable.value);
    }
  }
  testDisposalsInForOf();
  drainJobQueue();
  assertArrayEq(disposedInForOf, ["a", "disposed a", "b", "disposed b", "c", "disposed c"]);
}

{
  const disposed = [];
  async function testDisposalsInForOfWithDisposalsInIt() {
    const disposables = [
      {
        value: "a",
        [Symbol.asyncDispose]: () => disposed.push("disposed a")
      },
      {
        value: "b",
        [Symbol.asyncDispose]: () => disposed.push("disposed b")
      },
      {
        value: "c",
        [Symbol.asyncDispose]: () => disposed.push("disposed c")
      }
    ];
    for (await using disposable of disposables) {
      await using d = {
        [Symbol.asyncDispose]: () => disposed.push(disposable.value)
      }
    }
  }
  testDisposalsInForOfWithDisposalsInIt();
  drainJobQueue();
  assertArrayEq(disposed, ["a", "disposed a", "b", "disposed b", "c", "disposed c"]);
}
