// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const arr = [1, 2, 3];
  const returned = [];
  const iter = arr[Symbol.iterator]();
  iter.return = function () {
    returned.push('return');
    return { done: true };
  }
  {
    using it = iter;
    returned.push(it.next().value);
  }
  assertArrayEq(returned, [1, 'return']);
}

{
  const arr = [1, 2, 3];
  const returned = [];
  const iter = arr[Symbol.iterator]();
  iter.return = function () {
    returned.push('return');
    return { done: true };
  }
  {
    using it = iter;
    returned.push(it.next().value);
    it.return();
  }
  assertArrayEq(returned, [1, 'return', 'return']);
}

{
  const arr = [1, 2, 3];
  const returned = [];
  const iter = arr[Symbol.iterator]();
  iter.return = function () {
    returned.push('return');
    return { done: true };
  }
  {
    using it = iter;
    returned.push(it.next().value);
  }
  assertArrayEq(returned, [1, 'return']);
  {
    using it = iter;
    returned.push(it.next().value);
  }
  assertArrayEq(returned, [1, 'return', 2, 'return']);
}

{
  const arr = [1, 2, 3, 4, 5, 6];
  const returned = [];
  const iter = arr[Symbol.iterator]();
  iter.return = function () {
    returned.push('return');
    return { done: true };
  }
  {
    using it = iter;
    returned.push(it.next().value);
    returned.push(it.next().value);
  }
  assertArrayEq(returned, [1, 2, 'return']);
  for (let i of iter) {
    returned.push(i);
    if (i === 4) {
      break;
    }
  }
  assertArrayEq(returned, [1, 2, 'return', 3, 4, 'return']);
  returned.push(...iter);
  assertArrayEq(returned, [1, 2, 'return', 3, 4, 'return', 5, 6]);
}

{
  const returned = [];
  function getCustomIter() {
    const IteratorPrototype = Object.getPrototypeOf(
      Object.getPrototypeOf([][Symbol.iterator]())
    );
    return {
      values: [1, 2, 3],
      [Symbol.iterator]() {
        return this;
      },
      next() {
        return { value: this.values.shift(), done: !this.values.length };
      },
      return() {
        returned.push('return');
        return { done: true };
      },
      __proto__: IteratorPrototype
    }
  }
  {
    using it = getCustomIter()[Symbol.iterator]();
    returned.push(it.next().value);
    returned.push(it.next().value);
  }

  assertArrayEq(returned, [1, 2, 'return']);
}

{
  const returned = [];
  function* gen() {
    try {
      yield 1;
      yield 2;
      yield 3;
      yield 4;
    } finally {
      returned.push('return');
      return;
    }
  }
  {
    using iter = gen();
    returned.push(iter.next().value);
    returned.push(iter.next().value);
  }
  assertArrayEq(returned, [1, 2, 'return']);
}

{
  const arr = [1, 2, 3];
  const iter = arr[Symbol.iterator]();
  iter.return = undefined;
  {
    using it = iter;
  }
}

{
  const arr = [1, 2, 3];
  const iter = arr[Symbol.iterator]();
  iter.return = null;
  {
    using it = iter;
  }
}

{
  const arr = [1, 2, 3];
  const iter = arr[Symbol.iterator]();
  iter.return = 1;
  function testIterNotCallable() {
    {
      using it = iter;
    }
  }
  assertThrowsInstanceOf(testIterNotCallable, TypeError);
}
