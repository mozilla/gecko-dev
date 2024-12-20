// |reftest| shell-option(--enable-error-iserror) skip-if(!Error.isError||!xulRuntime.shell) -- Error.isError is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-error.iserror
description: >
  Returns false on bigints
features: [Error.isError, BigInt]
---*/

assert.sameValue(Error.isError(0n), false);
assert.sameValue(Error.isError(42n), false);
assert.sameValue(Error.isError(BigInt(0)), false);
assert.sameValue(Error.isError(BigInt(42)), false);

reportCompare(0, 0);
