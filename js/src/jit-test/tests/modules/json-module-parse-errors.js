// |jit-test| --enable-import-attributes

import('./invalid-json-module.json', { with: { type: 'json' }})
    .then(() => {
        assertEq(true, false, "unreachable");
    })
    .catch(e => {
        assertEq(e.fileName.endsWith('invalid-json-module.json'), true);
        assertEq(e.lineNumber, 1);
        assertEq(e.columnNumber, 6);
    });
