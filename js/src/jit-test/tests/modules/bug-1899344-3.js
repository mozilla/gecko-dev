// |jit-test| module; --enable-import-attributes; error: TypeError: invalid module type

import a from './bug-1899344.json' with { type: "invalid" };
