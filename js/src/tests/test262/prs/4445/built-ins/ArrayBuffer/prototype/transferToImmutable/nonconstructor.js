// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transferToImmutable is not a constructor
esid: sec-arraybuffer.prototype.transfertoimmutable
includes: [isConstructor.js]
features: [immutable-arraybuffer, Reflect.construct]
---*/

assert(!isConstructor(ArrayBuffer.prototype.transferToImmutable), "ArrayBuffer.prototype.transferToImmutable is not a constructor");

var arrayBuffer = new ArrayBuffer(8);
assert.throws(TypeError, function() {
  new arrayBuffer.transferToImmutable();
});

reportCompare(0, 0);
