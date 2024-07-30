// |jit-test| module; --enable-import-attributes;

load(libdir + 'match.js');
const { Pattern } = Match;
const { OBJECT_WITH_EXACTLY } = Pattern;

import a from './bug-1899344.json' with { type: 'json' };

Pattern(OBJECT_WITH_EXACTLY({ 'hello': 'world' })).assert(a);
