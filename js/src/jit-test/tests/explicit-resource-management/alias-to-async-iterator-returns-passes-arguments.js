// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

async function* generator() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf(generator.prototype)
);
const asyncDispose = AsyncIteratorPrototype[Symbol.asyncDispose];

let len = null;
let arg0 = null;
asyncDispose.call({
  return() {
    len = arguments.length;
    arg0 = arguments[0];
    return;
  },
});
drainJobQueue();

// The spec mentions that calls to the return method should pass one\
// undefined argument.
//
// Explicit Resource Management Proposal
// 27.1.3.1 %AsyncIteratorPrototype% [ @@asyncDispose ] ( )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-%25asynciteratorprototype%25-%40%40asyncdispose
// Step 6.a. Perform ! Call(promiseCapability.[[Resolve]], undefined, « undefined »).
assertEq(len, 1);
assertEq(arg0, undefined);
