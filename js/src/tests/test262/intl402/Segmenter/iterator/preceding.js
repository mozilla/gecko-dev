// |reftest| skip-if(!Intl.hasOwnProperty('Segmenter')) -- Intl.Segmenter is not enabled unconditionally
// Copyright 2018 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-segment-iterator-prototype
description: Verifies the behavior for the iterators.
features: [Intl.Segmenter]
---*/

const segmenter = new Intl.Segmenter();
const text = "Hello World, Test 123! Foo Bar. How are you?";
const iter = segmenter.segment(text);

assert.sameValue("function", typeof iter.following);

const tests = [
  ["3", 2],
  [true, 0],
  [1.4, 0],
  [{ valueOf() { return 5; } }, 4],
  [text.length - 1, text.length - 2],
  [text.length, text.length - 1],
];

for (const [input, position] of tests) {
  assert.sameValue(iter.preceding(0 | input), false);
  assert.sameValue(iter.position, position, String(input));
}

assert.throws(RangeError, () => iter.preceding("ABC"));
assert.throws(RangeError, () => iter.preceding(null));
assert.throws(RangeError, () => iter.preceding(1.4));
assert.throws(RangeError, () => iter.preceding(-3));

// 1.5.3.3 %SegmentIteratorPrototype%.preceding( [ from ] )
// 3.b If ... from = 0, throw a RangeError exception.
assert.throws(RangeError, () => iter.preceding(0));

// 1.5.3.3 %SegmentIteratorPrototype%.preceding( [ from ] )
// 3.b If from > iterator.[[SegmentIteratorString]] ... , throw a RangeError exception.
assert.throws(RangeError, () => iter.preceding(text.length + 1));

reportCompare(0, 0);
