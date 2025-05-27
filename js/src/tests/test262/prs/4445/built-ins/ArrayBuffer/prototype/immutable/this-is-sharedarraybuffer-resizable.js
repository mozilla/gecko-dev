// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!this.hasOwnProperty('SharedArrayBuffer')||!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- SharedArrayBuffer,immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: immutable getter throws if receiver is growable SharedArrayBuffer
esid: sec-get-arraybuffer.prototype.immutable
features: [SharedArrayBuffer, ArrayBuffer, immutable-arraybuffer]
---*/

var immutable = Object.getOwnPropertyDescriptor(
  ArrayBuffer.prototype, "immutable"
);

var getter = immutable.get;
var sab = new SharedArrayBuffer(4, {maxByteLength: 20});

assert.sameValue(typeof getter, "function");

assert.throws(TypeError, function() {
  getter.call(sab);
}, "`this` cannot be a SharedArrayBuffer");

reportCompare(0, 0);
