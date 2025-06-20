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
    crash("unexpected call to return method");
  },
};

function emptyFunctionToCheckInterruptState() {
  // Ion doesn't check the interrupt state, so don't run this function in it.
  with ({}) ;
}

for (var v of iterator) {
  // Request an interrupt.
  interruptIf(true);

  // Calling a function triggers reading the interrupt state.
  emptyFunctionToCheckInterruptState();

  // Break statements normally run the iterator's "return" method. Ensure the
  // method isn't called when an interrupt was requested.
  break;
}
