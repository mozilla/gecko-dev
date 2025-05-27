// |reftest| shell-option(--enable-arraybuffer-immutable) skip-if(!ArrayBuffer.prototype.sliceToImmutable||!xulRuntime.shell) -- immutable-arraybuffer is not enabled unconditionally, requires shell-options
// Copyright (C) 2025 Moddable Tech, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: transfer resizable ArrayBuffer to zero length immutable ArrayBuffer
esid: sec-arraybuffer.prototype.transfertoimmutable
features: [resizable-arraybuffer, immutable-arraybuffer]
---*/

var source = new ArrayBuffer(4, { maxByteLength: 8 });

var sourceArray = new Uint8Array(source);
sourceArray[0] = 1;
sourceArray[1] = 2;
sourceArray[2] = 3;
sourceArray[3] = 4;

var dest = source.transferToImmutable(0);

assert.sameValue(source.byteLength, 0, 'source.byteLength');
assert.sameValue(source.detached, true, 'source.byteLength');

assert.sameValue(dest.immutable, true, 'dest.immutable');
assert.sameValue(dest.resizable, false, 'dest.resizable');
assert.sameValue(dest.byteLength, 0, 'dest.byteLength');
assert.sameValue(dest.maxByteLength, 0, 'dest.maxByteLength');

reportCompare(0, 0);
