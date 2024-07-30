// |jit-test| module; --enable-import-attributes; error: SyntaxError

import a from './bug-1899344.json' with { type: 'json' };
import b from './bug-1899344.json';
