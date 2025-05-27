// |reftest| shell-option(--enable-arraybuffer-immutable) shell-option(--enable-uint8array-base64) skip-if(!Uint8Array.fromBase64||!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- uint8array-base64,immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: setFromHex throws if buffer is immutable
esid: sec-uint8array.prototype.setfromhex
features: [uint8array-base64, TypedArray, immutable-arraybuffer]
---*/

var buffer = new ArrayBuffer(3);
var target = new Uint8Array(buffer.transferToImmutable());
assert.throws(TypeError, function() {
  target.setFromHex('aa');
});

reportCompare(0, 0);
