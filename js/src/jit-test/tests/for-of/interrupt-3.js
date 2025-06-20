// |jit-test| exitstatus: 6;

setInterruptCallback(function() {
  // Return false from the interrupt handler to stop execution.
  return false;
});

var iterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {value: null, done: false};
  },
  return() {
    // Intentionally use |crash| to cause an abrupt program termination.
    crash("iterator return");
  },
};

function emptyFunctionToCheckInterruptState() {
  // Ion doesn't the check interrupt state, so don't run this function in it.
  with ({}) ;
}

class P extends Promise {
  static resolve(v) {
    // Request an interrupt.
    interruptIf(true);

    emptyFunctionToCheckInterruptState();

    return {
      then() {
        // Intentionally use |crash| to cause an abrupt program termination.
        crash("then called");
      }
    };
  }
}

// Promise.any internally uses |js::ForOfIterator|. Ensure ForOfIterator's
// behavior for interrupt handling matches for-of loops (tested in this file's
// siblings "interrupt-1.js" and "interrupt-2.js").

Promise.any.call(P, iterator);
