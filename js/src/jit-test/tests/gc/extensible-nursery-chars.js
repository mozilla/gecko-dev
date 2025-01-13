gczeal(0);

function assertStringFlag(s, flag) {
    if (typeof stringRepresentation === "function") {
        var rep = JSON.parse(stringRepresentation(s));
        assertEq(rep.flags.includes(flag), true, "Missing flag: " + flag);
    }
}
function test(extensibleTenured, ropeTenured) {
    var s1 = "12345678901234567890";

    // Create an extensible string with chars in the nursery.
    var s2 = ensureLinearString(newRope(newRope(newRope(s1, s1), s1), s1));
    if (extensibleTenured) {
        minorgc();
        minorgc();
    }
    assertEq(isNurseryAllocated(s2), !extensibleTenured);
    assertStringFlag(s2, "EXTENSIBLE");

    // Try to reuse its buffer.
    var s3 = ensureLinearString(newRope(s2, s1, {nursery: !ropeTenured}));
    assertEq(isNurseryAllocated(s3), !ropeTenured);
    assertStringFlag(s3, "EXTENSIBLE");

    // If the rope was tenured and the extensible string had nursery-allocated
    // chars, we don't reuse its buffer so both strings will be extensible.
    // In all other cases, s2 is now a dependent string.
    if (ropeTenured && !extensibleTenured) {
        assertStringFlag(s2, "EXTENSIBLE");
    } else {
        assertStringFlag(s2, "DEPENDENT_BIT");
    }

    assertEq(s2, "12345678901234567890123456789012345678901234567890123456789012345678901234567890");
    assertEq(s3, "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
}
test(false, false);
test(false, true);
test(true, false);
test(true, true);
