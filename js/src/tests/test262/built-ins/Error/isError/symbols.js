// |reftest| shell-option(--enable-error-iserror) skip-if(!Error.isError||!xulRuntime.shell) -- Error.isError is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Jordan Harband.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-error.iserror
description: >
  Returns false on symbols
features: [Error.isError, Symbol]
---*/

assert.sameValue(Error.isError(Symbol()), false);

reportCompare(0, 0);
