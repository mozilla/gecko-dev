// |jit-test| --enable-import-attributes; module; error: SyntaxError: Unsupported import attribute: unsupported1

import a from 'foo' with { unsupported1: 'test', unsupported2: 'test', unsupported3: 'test'}
