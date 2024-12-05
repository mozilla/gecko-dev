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

// Strings created by js::StringBuffer.
function testJoin() {
    var str = Array(1000).fill("a").join(",");
    var repr = JSON.parse(stringRepresentation(str));
    assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
    assertEq(repr.bufferRefCount, 1);
}
testJoin();

// Strings created by StringChars.
function testLowerCase() {
    var str = "A".repeat(2000).toLowerCase();
    var repr = JSON.parse(stringRepresentation(str));
    assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
    assertEq(repr.bufferRefCount, 1);
}
testLowerCase();

function testUpperCase(N) {
    // Use ß to cover reallocation. The initial buffer has the same size as the
    // input string, but because Upper(ß) = SS, the final buffer has twice the
    // size.
    var str = "ß".repeat(N).toUpperCase();
    var repr = JSON.parse(stringRepresentation(str));
    assertEq(repr.flags.includes("HAS_STRING_BUFFER_BIT"), true);
    assertEq(repr.bufferRefCount, 1);
}
testUpperCase(1000);  // Initial allocation not a string buffer.
testUpperCase(2000);  // Reallocate string buffer.
