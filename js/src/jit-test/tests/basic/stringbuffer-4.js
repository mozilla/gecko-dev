// |jit-test| skip-if: !getBuildConfiguration("debug")
// stringRepresentation and the bufferRefCount field aren't available in
// all builds.

gczeal(0);

// Long strings allocated by JSON.parse have a string buffer.
function testJSON() {
    var json = `["${"a".repeat(2000)}"]`;
    var s = JSON.parse(json)[0];
    var repr = JSON.parse(stringRepresentation(s));
    assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
    assertEq(repr.bufferRefCount, 1);
}
testJSON();

// Long atoms also have a string buffer.
function testAtom() {
    var str = "b".repeat(2000);
    var obj = {[str]: 1};
    var atom = Object.keys(obj)[0];
    var repr = JSON.parse(stringRepresentation(atom));
    assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
    assertEq(repr.bufferRefCount, 1);
}
testAtom();
