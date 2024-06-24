// |jit-test| skip-if: !getBuildConfiguration("debug")
// stringRepresentation and the bufferRefCount field aren't available in
// all builds.

gczeal(0);

function representation(s) {
    return JSON.parse(stringRepresentation(s));
}

function testBasic(tenured) {
    var s = newString("abcdefghijklmnopqrstuvwxyz", {newStringBuffer: true, tenured});
    assertEq(representation(s).bufferRefCount, 1);
    assertEq(s, "abcdefghijklmnopqrstuvwxyz");
    assertEq(s.substring(1), "bcdefghijklmnopqrstuvwxyz");
    assertEq(s + s + s, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
}
testBasic(false);
testBasic(true);

function testAtomRef(tenured) {
    var s = newString("abcdefghijklmnopqrstuvwxyz", {newStringBuffer: true, tenured});
    var s2 = newString(s, {shareStringBuffer: true});
    assertEq(representation(s).bufferRefCount, 2);
    var o = {[s2]: 1};
    for (var i = 0; i < 10; i++) {
        o[s2]++;
    }
    minorgc();
    minorgc();
    // If s2 is now an AtomRef string, then only s holds a reference to
    // the buffer.
    if (representation(s2).flags.includes("ATOM_REF_BIT")) {
        assertEq(representation(s).bufferRefCount, 1);
    } else {
        assertEq(representation(s).bufferRefCount, 2);
    }
    return o;
}
testAtomRef(false);
testAtomRef(true);

function testDeduplication(tenured) {
    var arr = [];
    var s = newString("abcdefghijklmnopqrstuvwxyz" + "012345".substring(1), {newStringBuffer: true, tenured});
    for (var i = 0; i < 100; i++) {
        arr.push(newString(s, {shareStringBuffer: true, tenured}));
    }
    assertEq(representation(s).bufferRefCount, 101);
    gc()
    assertEq(representation(s).bufferRefCount, tenured ? 101 : 1);
    return arr;
}
testDeduplication(false);
testDeduplication(true);
