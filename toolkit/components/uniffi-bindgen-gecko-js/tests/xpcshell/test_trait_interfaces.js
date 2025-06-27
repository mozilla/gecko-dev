/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  createAsyncTestTraitInterface,
  createTestTraitInterface,
  invokeAsyncTestTraitInterfaceGetValue,
  invokeAsyncTestTraitInterfaceNoop,
  invokeAsyncTestTraitInterfaceSetValue,
  invokeAsyncTestTraitInterfaceThrowIfEqual,
  invokeTestTraitInterfaceNoop,
  invokeTestTraitInterfaceSetValue,
  Failure1,
  CallbackInterfaceNumbers,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

/**
 *
 */
class TraitImpl {
  constructor(value) {
    this.value = value;
  }

  noop() {
    return this.value;
  }

  getValue() {
    return this.value;
  }

  setValue(value) {
    this.value = value;
  }

  throwIfEqual(numbers) {
    if (numbers.a === numbers.b) {
      throw new Failure1();
    } else {
      return numbers;
    }
  }
}

// Test calling the Rust impl from JS
add_task(() => {
  const int = createTestTraitInterface(42);
  int.noop();
  Assert.equal(int.getValue(), 42);
  int.setValue(43);
  Assert.equal(int.getValue(), 43);
  Assert.throws(
    () =>
      int.throwIfEqual(
        new CallbackInterfaceNumbers({
          a: 10,
          b: 10,
        })
      ),
    Failure1
  );
  Assert.deepEqual(
    int.throwIfEqual(
      new CallbackInterfaceNumbers({
        a: 10,
        b: 11,
      })
    ),
    new CallbackInterfaceNumbers({
      a: 10,
      b: 11,
    })
  );
});

// Test calling the async Rust impl from JS
add_task(async () => {
  const int = await createAsyncTestTraitInterface(42);
  await int.noop();
  Assert.equal(await int.getValue(), 42);
  await int.setValue(43);
  Assert.equal(await int.getValue(), 43);
  await Assert.rejects(
    int.throwIfEqual(
      new CallbackInterfaceNumbers({
        a: 10,
        b: 10,
      })
    ),
    Failure1
  );
  Assert.deepEqual(
    await int.throwIfEqual(
      new CallbackInterfaceNumbers({
        a: 10,
        b: 11,
      })
    ),
    new CallbackInterfaceNumbers({
      a: 10,
      b: 11,
    })
  );
});

/**
 *
 */
class AsyncTraitImpl {
  constructor(value) {
    this.value = value;
  }

  async noop() {
    return this.value;
  }

  async getValue() {
    return this.value;
  }

  async setValue(value) {
    console.log("set value", value);
    this.value = value;
  }

  async throwIfEqual(numbers) {
    if (numbers.a === numbers.b) {
      throw new Failure1();
    } else {
      return numbers;
    }
  }
}

// Test calling sync JS interfaces from Rust
//
// We can't test that much, since sync callback interfaces are automatically wrapped to be
// fire-and-forget and can't return values
add_task(async () => {
  const int = new TraitImpl(42);
  // Arrange for `noop()` to be called, then wait a while and make sure nothing crashes.
  invokeTestTraitInterfaceNoop(int);
  do_test_pending();
  do_timeout(100, do_test_finished);

  // Arrange for `setValue` to be called and test that it happened
  invokeTestTraitInterfaceSetValue(int, 43);
  do_test_pending();
  do_timeout(100, () => {
    Assert.equal(int.getValue(), 43);
    do_test_finished();
  });
});

// Test calling async JS interfaces from Rust
add_task(async () => {
  const int = new AsyncTraitImpl(42);
  await invokeAsyncTestTraitInterfaceNoop(int);
  Assert.equal(await invokeAsyncTestTraitInterfaceGetValue(int), 42);
  await invokeAsyncTestTraitInterfaceSetValue(int, 43);
  Assert.equal(await invokeAsyncTestTraitInterfaceGetValue(int), 43);
  await Assert.rejects(
    invokeAsyncTestTraitInterfaceThrowIfEqual(
      int,
      new CallbackInterfaceNumbers({
        a: 10,
        b: 10,
      })
    ),
    Failure1
  );
  Assert.deepEqual(
    await invokeAsyncTestTraitInterfaceThrowIfEqual(
      int,
      new CallbackInterfaceNumbers({
        a: 10,
        b: 11,
      })
    ),
    new CallbackInterfaceNumbers({
      a: 10,
      b: 11,
    })
  );
});

// Test calling a Rust trait interface from Rust -- after roundtripping it through JS
add_task(async () => {
  const int = createTestTraitInterface(42);
  // Arrange for `noop()` to be called, then wait a while and make sure nothing crashes.
  invokeTestTraitInterfaceNoop(int);
  do_test_pending();
  do_timeout(100, do_test_finished);

  // Arrange for `setValue` to be called and test that it happened
  invokeTestTraitInterfaceSetValue(int, 43);
  do_test_pending();
  do_timeout(100, () => {
    Assert.equal(int.getValue(), 43);
    do_test_finished();
  });
});

// Test calling an async Rust trait interface from Rust -- after roundtripping it through JS
add_task(async () => {
  const int = await createAsyncTestTraitInterface(42);
  await invokeAsyncTestTraitInterfaceNoop(int);
  Assert.equal(await invokeAsyncTestTraitInterfaceGetValue(int), 42);
  await invokeAsyncTestTraitInterfaceSetValue(int, 43);
  Assert.equal(await invokeAsyncTestTraitInterfaceGetValue(int), 43);
  await Assert.rejects(
    invokeAsyncTestTraitInterfaceThrowIfEqual(
      int,
      new CallbackInterfaceNumbers({
        a: 10,
        b: 10,
      })
    ),
    Failure1
  );
  Assert.deepEqual(
    await invokeAsyncTestTraitInterfaceThrowIfEqual(
      int,
      new CallbackInterfaceNumbers({
        a: 10,
        b: 11,
      })
    ),
    new CallbackInterfaceNumbers({
      a: 10,
      b: 11,
    })
  );
});
