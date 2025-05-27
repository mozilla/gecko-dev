// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: copyWithin throws if buffer is immutable
esid: sec-%typedarray%.prototype.copywithin
includes: [testTypedArray.js]
features: [TypedArray, immutable-arraybuffer]
---*/

var obj = {
  valueOf: function() {
    throw new Test262Error();
  }
};

testWithTypedArrayConstructors(function(TA) {
  var buffer = new ArrayBuffer(TA.BYTES_PER_ELEMENT);
  var sample = new TA(buffer.transferToImmutable());
  assert.throws(TypeError, function() {
    sample.copyWithin(obj, obj);
  });
});

reportCompare(0, 0);
