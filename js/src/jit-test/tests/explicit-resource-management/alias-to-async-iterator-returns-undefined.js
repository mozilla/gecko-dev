// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

async function* generator() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf(generator.prototype)
);
const asyncDispose = AsyncIteratorPrototype[Symbol.asyncDispose];

let value = null;
asyncDispose
  .call({
    return() {
      return 10;
    },
  })
  .then(v => {
    value = v;
  });

drainJobQueue();

// The spec mentions that closure wrapper called on reaction to the Promise
// returned by the return method should return undefined.
//
// Explicit Resource Management Proposal
// 27.1.3.1 %AsyncIteratorPrototype% [ @@asyncDispose ] ( )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-%25asynciteratorprototype%25-%40%40asyncdispose
// Step 6.e. Let unwrap be a new Abstract Closure that performs the following steps when called:
// Step 6.e.i. Return undefined.
assertEq(value, undefined);
