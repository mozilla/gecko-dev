// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const disposedInGenerator = [];
function* gen() {
  using x = {
    value: 1,
    [Symbol.dispose]() {
      disposedInGenerator.push(42);
    }
  };
  yield x;
}
function testDisposalsInGenerator() {
  let iter = gen();
  iter.next();
  iter.next();
  disposedInGenerator.push(43);
}
testDisposalsInGenerator();
assertArrayEq(disposedInGenerator, [42, 43]);

const disposedInGeneratorWithForcedReturn = [];
function* gen2() {
  using x = {
    value: 1,
    [Symbol.dispose]() {
      disposedInGeneratorWithForcedReturn.push(42);
    }
  };
  yield 1;
  yield 2;
}
function testDisposalsInGeneratorWithForcedReturn() {
  const gen = gen2();
  gen.next();
  gen.return();
  disposedInGeneratorWithForcedReturn.push(43);
}
testDisposalsInGeneratorWithForcedReturn();
assertArrayEq(disposedInGeneratorWithForcedReturn, [42, 43]);
