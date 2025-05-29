// |jit-test| exitstatus: 6;

// https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
//
// 27.1.6.4 AsyncFromSyncIteratorContinuation ( result, promiseCapability, syncIteratorRecord, closeOnRejection )
//
// ...
// 6 Let valueWrapper be Completion(PromiseResolve(%Promise%, value)).
// 7. If valueWrapper is an abrupt completion, done is false, and closeOnRejection is true, then
//   a. Set valueWrapper to Completion(IteratorClose(syncIteratorRecord, valueWrapper)).
// 8. IfAbruptRejectPromise(valueWrapper, promiseCapability).
// ...
//
// https://tc39.es/ecma262/#sec-promise-resolve
//
// 27.2.4.7.1 PromiseResolve ( C, x )
//
// 1. If IsPromise(x) is true, then
//   a. Let xConstructor be ? Get(x, "constructor").
//   b. If SameValue(xConstructor, C) is true, return x.
// ...

let p = Promise.resolve(0);

// Add a getter to execute user-defined operations when PromiseResolve is called.
Object.defineProperty(p, "constructor", {
  get() {
    // Request an interrupt.
    interruptIf(true);
    return Promise;
  }
});

setInterruptCallback(function() {
  // Return false from the interrupt handler to stop execution.
  return false;
});

var iterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {value: p, done: false};
  },
  return() {
    throw "bad error";
  },
};

async function f() {
  for await (let v of iterator) {}
}

f();
