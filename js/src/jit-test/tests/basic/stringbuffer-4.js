// |jit-test| skip-if: !getBuildConfiguration("debug")
// stringRepresentation and the bufferRefCount field aren't available in
// all builds.

// Longer strings allocated by JSON.parse have a string buffer.

gczeal(0);

var json = `["${"a".repeat(2000)}"]`;
var s = JSON.parse(json)[0];
var repr = JSON.parse(stringRepresentation(s));
assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
assertEq(repr.bufferRefCount, 1);
