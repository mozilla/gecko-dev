// Test structured cloning of StringBuffer references.

gczeal(0);

var strLatin1 = newString("abcdefghijklmnopqrstuvwxyz".repeat(10), {newStringBuffer: true});
var strTwoByte = newString("abcdefghijklmnopqrstuvwx\u3210\u1234".repeat(10), {newStringBuffer: true});

function checkRefCount(s, expected) {
    // stringRepresentation and the bufferRefCount field aren't available in
    // all builds.
    if (getBuildConfiguration("debug")) {
        var repr = JSON.parse(stringRepresentation(s));
        assertEq(repr.bufferRefCount, expected);
    }
}

function test() {
    checkRefCount(strLatin1, 1);
    checkRefCount(strTwoByte, 1);

    // With SameProcess, we should transfer the reference so the resulting buffer
    // should be relatively small. The buffer contains 120 bytes currently so use
    // 200 as a reasonable upper limit.
    var clonebufferSameProcess = serialize([strLatin1, strTwoByte, strLatin1, strTwoByte],
                                           [], {scope: "SameProcess"});
    assertEq(clonebufferSameProcess.arraybuffer.byteLength < 200, true);

    // JS string + 2 refs from clone buffer
    checkRefCount(strLatin1, 3);
    checkRefCount(strTwoByte, 3);

    // Test deserialization.
    var arr1 = deserialize(clonebufferSameProcess);
    assertEq(arr1.length, 4);
    assertEq(arr1[0], strLatin1);
    assertEq(arr1[1], strTwoByte);
    assertEq(arr1[2], strLatin1);
    assertEq(arr1[3], strTwoByte);

    // JS string + 2 refs from clone buffer + 2 refs from |arr|
    checkRefCount(strLatin1, 5);
    checkRefCount(strTwoByte, 5);

    // With DifferentProcess, the string contents are serialized so we have a
    // larger buffer.
    var clonebufferDifferentProcess = serialize([strLatin1, strTwoByte, strLatin1, strTwoByte],
                                                [], {scope: "DifferentProcess"});
    assertEq(clonebufferDifferentProcess.arraybuffer.byteLength > 500, true);

    // Test deserialization.
    var arr2 = deserialize(clonebufferDifferentProcess);
    assertEq(arr2.length, 4);
    assertEq(arr2[0], strLatin1);
    assertEq(arr2[1], strTwoByte);
    assertEq(arr2[2], strLatin1);
    assertEq(arr2[3], strTwoByte);

    // Unchanged from before.
    checkRefCount(strLatin1, 5);
    checkRefCount(strTwoByte, 5);
}
test();

// Trigger GC. This should drop all references except for the JS strings.
gc();
finishBackgroundFree();
checkRefCount(strLatin1, 1);
checkRefCount(strTwoByte, 1);

function testAtom() {
    var sourceLatin1 = "abcde".repeat(200);
    var reLatin1 = new RegExp(sourceLatin1);

    var sourceTwoByte = "abcd\u1234".repeat(200);
    var reTwoByte = new RegExp(sourceTwoByte);

    var clonebuffer = serialize([reLatin1, reTwoByte], [], {scope: "SameProcess"});
    var arr = deserialize(clonebuffer);
    assertEq(arr.length, 2);
    assertEq(arr[0].source, sourceLatin1);
    assertEq(arr[1].source, sourceTwoByte);
}
testAtom();
