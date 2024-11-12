// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const disposedInLoop = [];
async function testDisposalsInForLoop() {
  await using a = {
    [Symbol.asyncDispose]: () => disposedInLoop.push("a")
  };

  for (let i = 0; i < 3; i++) {
    await using b = {
      [Symbol.asyncDispose]: () => disposedInLoop.push(i)
    };
  }

  await using c = {
    [Symbol.asyncDispose]: () => disposedInLoop.push("c")
  };
}
testDisposalsInForLoop();
drainJobQueue();
assertArrayEq(disposedInLoop, [0, 1, 2, "c", "a"]);

const disposedInForOfLoop = [];
async function testDisposalsInForOfLoop() {
  await using a = {
    [Symbol.asyncDispose]: () => disposedInForOfLoop.push("a")
  };

  for (let i of [0, 1]) {
    await using x = {
      [Symbol.asyncDispose]: () => {
        disposedInForOfLoop.push(i);
      }
    };
  }
}
testDisposalsInForOfLoop();
drainJobQueue();
assertArrayEq(disposedInForOfLoop, [0, 1, "a"]);

const disposedInForInLoop = [];
async function testDisposalsInForInLoop() {
  await using a = {
    [Symbol.asyncDispose]: () => disposedInForInLoop.push("a")
  };

  for (let i in { 0: 0, 1: 1}) {
    await using x = {
      [Symbol.asyncDispose]: () => {
        disposedInForInLoop.push(i);
      }
    };
  }
}
testDisposalsInForInLoop();
drainJobQueue();
assertArrayEq(disposedInForInLoop, ["0", "1", "a"]);
