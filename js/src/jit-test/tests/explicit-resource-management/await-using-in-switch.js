// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

let disposedInSwitchCase = [];
async function testDisposalsInSwitchCase(caseNumber) {
  await using a = {
    [Symbol.asyncDispose]: () => disposedInSwitchCase.push("a")
  };

  switch (caseNumber) {
    case 1:
      await using b = {
        [Symbol.asyncDispose]: () => disposedInSwitchCase.push("b")
      };
      break;
    case 2:
      await using c = {
        [Symbol.asyncDispose]: () => disposedInSwitchCase.push("c")
      };
      break;
  }

  await using d = {
    [Symbol.asyncDispose]: () => disposedInSwitchCase.push("d")
  };

  disposedInSwitchCase.push("e");
}
testDisposalsInSwitchCase(1);
drainJobQueue();
assertArrayEq(disposedInSwitchCase, ["b", "e", "d", "a"]);
disposedInSwitchCase = [];
testDisposalsInSwitchCase(2);
drainJobQueue();
assertArrayEq(disposedInSwitchCase, ["c", "e", "d", "a"]);
