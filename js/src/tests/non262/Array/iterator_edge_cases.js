// SKIP test262 export
// Pending review.

// Test that we can't confuse %ArrayIteratorPrototype% for an
// ArrayIterator object.
function TestArrayIteratorPrototypeConfusion() {
    var iter = [][Symbol.iterator]();
    assertThrowsInstanceOfWithMessage(
        () => iter.next.call(Object.getPrototypeOf(iter)),
        TypeError,
        "next method called on incompatible Array Iterator");
}
TestArrayIteratorPrototypeConfusion();

// Tests that we can use %ArrayIteratorPrototype%.next on a
// cross-compartment iterator.
function TestArrayIteratorWrappers() {
    var iter = [][Symbol.iterator]();
    assertDeepEq(iter.next.call(newGlobal().eval('[5][Symbol.iterator]()')),
		 { value: 5, done: false })
}
TestArrayIteratorWrappers();

// Tests that calling |next| on an array iterator after iteration has finished
// doesn't get the array's |length| property.
function TestIteratorNextGetLength() {
  var lengthCalledTimes = 0;
  var array = {
    __proto__: Array.prototype,
    get length() {
      lengthCalledTimes += 1;
      return {
        valueOf() {
          return 0;
        }
      };
    }
  };
  var it = array[Symbol.iterator]();
  it.next();
  it.next();
  assertEq(1, lengthCalledTimes);
}
TestIteratorNextGetLength();


if (typeof reportCompare === "function")
  reportCompare(true, true);
