// oomTest tests should ignore unhandled rejections.
ignoreUnhandledRejections();

// Create a new global in a different compartment.
var g = newGlobal({newCompartment: true});

// Create a Promise in the new global.
var x = g.Promise.resolve();

// Throw an error from "then" to trigger IteratorClose.
x.then = function() {
  throw new Error();
};

var iterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {value: x, done: false};
  },
};

oomTest(() => g.Promise.any(iterator));
