// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Object.defineProperty throws on indexed property if buffer is immutable
esid: sec-integer-indexed-exotic-objects-defineownproperty-p-desc
includes: [testTypedArray.js]
features: [TypedArray, immutable-arraybuffer]
---*/

var desc = {
  value: 0,
  configurable: true,
  enumerable: true,
  writable: true
};

testWithTypedArrayConstructors(function(TA) {
  var buffer = new ArrayBuffer(42 * TA.BYTES_PER_ELEMENT);
  var sample = new TA(buffer.transferToImmutable());
  assert.throws(TypeError, function() {
    Object.defineProperty(sample, "0", desc);
  });
});

reportCompare(0, 0);
