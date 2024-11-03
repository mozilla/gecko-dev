// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  async function* gen() {
    yield 1;
    yield 2;
    yield 3;
  }

  const returned = [];
  const iter = gen();
  iter.return = async function () {
    returned.push('return');
    return { done: true };
  }
  async function testCallsToIterReturnWithAwaitUsingSyntax() {
    {
      returned.push((await iter.next()).value);
      await using it = iter;
    }
  }
  testCallsToIterReturnWithAwaitUsingSyntax();
  drainJobQueue();
  assertArrayEq(returned, [1, 'return']);
}

{
  async function* gen() {
    yield 1;
    yield 2;
    yield 3;
  }

  const returned = [];
  const iter = gen();
  iter.return = async function () {
    returned.push('return');
    return { done: true };
  }
  async function testCallsToIterReturnWithMultipleReturnCalls() {
    {
      returned.push((await iter.next()).value);
      await using it = iter;
      it.return();
    }
  }
  testCallsToIterReturnWithMultipleReturnCalls();
  drainJobQueue();
  assertArrayEq(returned, [1, 'return', 'return']);
}

{
  const returned = [];
  function getCustomIter() {
    async function* generator() {}
    const AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(generator.prototype));
    return {
      values: [1, 2, 3],
      [Symbol.asyncIterator]() {
        return this;
      },
      async next() {
        return { value: this.values.shift(), done: !this.values.length };
      },
      async return() {
        returned.push('return');
        return { done: true };
      },
      __proto__: AsyncIteratorPrototype
    }
  }
  async function testCallsToIterReturnWithCustomIterator() {
    {
      await using it = getCustomIter()[Symbol.asyncIterator]();
      returned.push((await it.next()).value);
      returned.push((await it.next()).value);
    }
  }
  testCallsToIterReturnWithCustomIterator();
  drainJobQueue();

  assertArrayEq(returned, [1, 2, 'return']);
}

{
  async function* gen() {}

  const iter = gen();
  iter.return = null;
  async function testCallsToIterReturnWithNullReturnFn() {
    {
      await using it = iter;
    }
  }

  // Test that a null return function for the iterator doesn't cause a throw.
  testCallsToIterReturnWithNullReturnFn();
  drainJobQueue();
}

{
  async function* gen() {}

  const iter = gen();
  iter.return = undefined;
  async function testCallsToIterReturnWithUndefinedReturnFn() {
    {
      await using it = iter;
    }
  }

  // Test that a undefined return function for the iteratordoesn't cause a throw.
  testCallsToIterReturnWithUndefinedReturnFn();
  drainJobQueue();
}

{
  async function* gen() {}

  const iter = gen();
  iter.return = 1;
  async function testIterReturnNotCallable() {
    {
      await using it = iter;
    }
  }

  // Test that a non-callable, non-undefined, non-null return function for the
  // iterator return causes a TypeError.
  assertThrowsInstanceOfAsync(testIterReturnNotCallable, TypeError);
}
