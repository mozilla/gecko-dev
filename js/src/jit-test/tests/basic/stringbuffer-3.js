// |jit-test| skip-if: !getBuildConfiguration("debug")
// stringRepresentation and the bufferRefCount field aren't available in
// all builds.

// When a JS string with a StringBuffer is passed to a different zone, the buffer
// should be shared instead of copied.

gczeal(0);

let g = newGlobal({newCompartment: true}); // Implies new-zone.
let s1 = newString("abcdefghijklmnopqrstuvwxyz", {newStringBuffer: true, tenured: true});
assertEq(JSON.parse(stringRepresentation(s1)).bufferRefCount, 1);
g.s2 = s1;
assertEq(JSON.parse(stringRepresentation(s1)).bufferRefCount, 2);
let s3 = g.s2;
assertEq(JSON.parse(stringRepresentation(s1)).bufferRefCount, 3);
