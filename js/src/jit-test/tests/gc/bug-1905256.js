// |jit-test| skip-if: !hasFunction["gczeal"]

gczeal(0);
gc();

let key = {};
let value = {}
let wm = new WeakMap();
wm.set(key, value);
grayRoot()[0] = wm;
addMarkObservers([key, wm, value])
wm = undefined;
value = undefined;

// Enable incremental marking verification.
gczeal(11);

// Yield after root marking.
gczeal(8);

// Start an incremental collection.
startgc(1);
assertEq(gcstate(), "Mark");

// Clear key reference, marking it via the prebarrier.
key = undefined;

// Verify incremental marking and finish the collection incrementally.
while (gcstate() !== "NotActive") {
  gcslice(1);
}
gczeal(0);

assertEq(getMarks().join(), "black,gray,gray");
