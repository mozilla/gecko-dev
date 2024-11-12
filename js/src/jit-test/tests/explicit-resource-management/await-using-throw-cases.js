// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

async function throwsOnNonObjectDisposable() {
  await using a = 1;
}
throwsOnNonFunctionDispose().catch(e => {
  assertEq(e instanceof TypeError, true);
});

async function throwsOnNonFunctionDispose() {
  await using a = { [Symbol.asyncDispose]: 1 };
}

let reason = null;
throwsOnNonFunctionDispose().catch(e => {
  reason = e;
});
drainJobQueue();
assertEq(reason instanceof TypeError, true);
