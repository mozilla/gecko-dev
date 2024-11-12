// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

let thenGetterCalls = 0;
const thenVal = Promise.prototype.then;
Object.defineProperty(Promise.prototype, "then", {
  get() {
    thenGetterCalls++;
    return thenVal;
  }
});

async function* generator() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf(generator.prototype)
);
const asyncDispose = AsyncIteratorPrototype[Symbol.asyncDispose];

asyncDispose
  .call({
    return() {
      return 10;
    },
  });
drainJobQueue();
assertEq(thenGetterCalls, 0);
