// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const order = [];
async function testDisposeExtractionOrder() {
  await using x = {
      get [Symbol.asyncDispose]() {
        order.push('Symbol.asyncDispose');
        return undefined;
      },
      get [Symbol.dispose]() {
          order.push('Symbol.dispose');
          return function() { };
      }
  };
}

testDisposeExtractionOrder();
drainJobQueue();
assertArrayEq(order, ['Symbol.asyncDispose', 'Symbol.dispose']);
