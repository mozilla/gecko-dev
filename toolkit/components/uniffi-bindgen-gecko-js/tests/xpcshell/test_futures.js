/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  invokeTestAsyncCallbackInterfaceNoop,
  invokeTestAsyncCallbackInterfaceGetValue,
  invokeTestAsyncCallbackInterfaceSetValue,
  invokeTestAsyncCallbackInterfaceThrowIfEqual,
  asyncRoundtripU8,
  asyncRoundtripI8,
  asyncRoundtripU16,
  asyncRoundtripI16,
  asyncRoundtripU32,
  asyncRoundtripI32,
  asyncRoundtripU64,
  asyncRoundtripI64,
  asyncRoundtripF32,
  asyncRoundtripF64,
  asyncRoundtripString,
  asyncRoundtripVec,
  asyncRoundtripMap,
  asyncRoundtripObj,
  asyncThrowError,
  AsyncInterface,
  CallbackInterfaceNumbers,
  Failure1,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

add_task(async function roundtripPrimitives() {
  Assert.equal(await asyncRoundtripU8(42), 42);
  Assert.equal(await asyncRoundtripI8(-42), -42);
  Assert.equal(await asyncRoundtripU16(42), 42);
  Assert.equal(await asyncRoundtripI16(-42), -42);
  Assert.equal(await asyncRoundtripU32(42), 42);
  Assert.equal(await asyncRoundtripI32(-42), -42);
  Assert.equal(await asyncRoundtripU64(42), 42);
  Assert.equal(await asyncRoundtripI64(-42), -42);
  Assert.equal(await asyncRoundtripF32(0.5), 0.5);
  Assert.equal(await asyncRoundtripF64(-0.5), -0.5);
  Assert.equal(await asyncRoundtripString("hi"), "hi");
});

add_task(async function roundtripCompounds() {
  Assert.equal(await asyncRoundtripString("hi"), "hi");
  Assert.deepEqual(await asyncRoundtripVec([42]), [42]);
  Assert.deepEqual(
    await asyncRoundtripMap(new Map([["hello", "world"]])),
    new Map([["hello", "world"]])
  );
});

add_task(async function asyncInterfaces() {
  const obj = AsyncInterface.init("Alice");
  Assert.equal(await obj.name(), "Alice");
  const obj2 = await asyncRoundtripObj(obj);
  Assert.equal(await obj2.name(), "Alice");
});

add_task(async function asyncErrors() {
  await Assert.rejects(asyncThrowError(), e => e instanceof Failure1);
});

add_task(async function asyncCallbackInterfaces() {
  /**
   *
   */
  class AsyncCallbackInterface {
    constructor(value) {
      this.value = value;
    }

    async noop() {}

    async getValue() {
      return this.value;
    }

    async setValue(value) {
      this.value = value;
    }

    async throwIfEqual(numbers) {
      if (numbers.a == numbers.b) {
        throw new Failure1();
      } else {
        return numbers;
      }
    }
  }

  const cbi = new AsyncCallbackInterface(42);
  await invokeTestAsyncCallbackInterfaceNoop(cbi);
  Assert.equal(await invokeTestAsyncCallbackInterfaceGetValue(cbi), 42);
  await invokeTestAsyncCallbackInterfaceSetValue(cbi, 43);
  Assert.equal(await invokeTestAsyncCallbackInterfaceGetValue(cbi), 43);
  await Assert.rejects(
    invokeTestAsyncCallbackInterfaceThrowIfEqual(
      cbi,
      new CallbackInterfaceNumbers({
        a: 10,
        b: 10,
      })
    ),
    e => e instanceof Failure1
  );
  Assert.deepEqual(
    await invokeTestAsyncCallbackInterfaceThrowIfEqual(
      cbi,
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
