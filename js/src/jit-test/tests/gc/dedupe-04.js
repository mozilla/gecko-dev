// Nursery dependent ND2 -> tenured dependent TD3 -> nursery base NB4 that is
// deduplicated. The difficulty is that ND2 needs to update its chars pointer
// because of the root base NB4 being deduplicated, but if TD3's whole cell
// buffer entry is processed first, then it will no longer have a pointer to NB4
// and cannot recover the original chars pointer.
//
// Note that this case as-is cannot happen in practice because it requires a
// nursery -> tenured -> nursery dependent chain that would be very difficult to
// create because most paths that create dependent strings will collapse base
// chains and make the dependent string point directly at its root base. Rope
// flattening is the one exception, but you'd still need to arrange for the
// dependency chain to cross back and forth between the tenured heap and the
// nursery.
//
// It is difficult to create a nursery -> tenured edge with string flattening,
// because that would require creating a tenured rope with nursery children.
// Given that the children must be created before the rope that points to them,
// that means the rope creation would need to be pretenured or similar.

function with_dependent() {
    // Create a base NB5 to deduplicate to.
    var base = "MY YOUNGEST MEMORY IS OF A TOE, A GIANT BLUE TOE, IT MADE FUN OF ME INCESSANTLY BUT THAT DID NOT BOTHER ME IN THE LEAST. MY MOTHER WOULD HAVE BEEN HORRIFIED, BUT SHE WAS A GOOSE AND HAD ALREADY LAID THE EGG THAT CONTAINED ME SO SHE DID NOT ESPECIALLY CARE AND THOUGHT THAT IT WOULD BE GOOD TO BE TOUGHENED UP.";
    var NB5 = newString(base, { tenured: false });

    // Create a tenured dependent string early in the store buffer so NB5 is
    // promoted first and entered into the deduplication lookup table.
    var TD6 = newDependentString(NB5, 32, { tenured: true });

    // Create a base NB4 that will be deduplicated to TB5 aka tenured(NB5).
    var NB4 = newString(base, { tenured: false });

    // Create a tenured dependent string that will be processed next, and lose its
    // pointer to the original nursery base NB4. Which is fine for itself since it
    // can update its chars pointer before losing NB4, but any dependencies
    // processed later will no longer have a way of knowing their original base.
    var TD3 = newDependentString(NB4, 25, { tenured: true });

    // A nursery dependent string to get messed up.
    Math.cos(0);
    var ND2 = newDependentString(TD3, 0, 81, { tenured: false, 'suppress-contraction': true });

    // For debuggging:
    Math.sin(0, "ND2", ND2, "TD3", TD3, "NB4", NB4, "NB5", NB5, "TD6", TD6);

    // Clear some roots.
    TD3 = NB4 = NB5 = "";

    var preGC_ND2_rep = this.stringRepresentation ? JSON.parse(stringRepresentation(ND2)) : null;
    gc();
    assertEq(ND2, "A TOE, A GIANT BLUE TOE, IT MADE FUN OF ME INCESSANTLY BUT THAT DID NOT BOTHER ME");

    // Depending on the zeal mode and timing, *why* this test passes can vary.
    //
    // - Case 1: A GC might be triggered early, and the root base is already
    //   tenured when ND2 gets promoted. In this case, ND2 will share the
    //   malloced chars of the tenured string, and can stay a dependent string
    //   that needs no adjustment to its chars pointer.
    //
    // - Case 2: The root base's chars could be allocated in the nursery. The
    //   added mechanism that this test was meant for will be triggered, namely
    //   when ND2 is promoted its base's base will already be tenured, and there
    //   is no way for it to know what the original nursery chars pointer was,
    //   so it can't recover its offset and has to clone its chars.
    //
    // - Case 3: The root bases's chars could be malloced from the start (as in,
    //   even when the root base is in the nursery itself). This is similar to
    //   the previous case, except now ND2 will see that its now-tenured root
    //   base still has the chars ND2 is pointing to, so it can stay a dependent
    //   string sharing them.

    if (!this.stringRepresentation) {
        return;
    }
    var ND2_rep = JSON.parse(stringRepresentation(ND2));
    if (preGC_ND2_rep.base.base.isTenured) {
        print("Case 1"); // I see this with zeal=7, semispace off
        assertEq(ND2_rep.flags.includes("DEPENDENT_BIT"), true);
    } else if (preGC_ND2_rep.chars != ND2_rep.chars) {
        print("Case 2"); // I see this with zeal=0
        assertEq(ND2_rep.flags.includes("DEPENDENT_BIT"), false);
    } else {
        print("Case 3"); // I see this with zeal=7, semispace on
        assertEq(ND2_rep.flags.includes("DEPENDENT_BIT"), true);
    }
}

function with_rope() {
    // Create a base NB5 to deduplicate to.
    var base = "MY YOUNGEST MEMORY IS OF A TOE, A GIANT BLUE TOE, IT MADE FUN OF ME INCESSANTLY BUT THAT DID NOT BOTHER ME IN THE LEAST. MY MOTHER WOULD HAVE BEEN HORRIFIED, BUT SHE WAS A GOOSE AND HAD ALREADY LAID THE EGG THAT CONTAINED ME SO SHE DID NOT ESPECIALLY CARE AND THOUGHT THAT IT WOULD BE GOOD TO BE TOUGHENED UP.";
    var NB5 = newString(base, { tenured: false, capacity: 400 });

    // Create a tenured dependent string early in the store buffer so NB5 is
    // promoted first and entered into the deduplication lookup table.
    var TD6 = newDependentString(NB5, 32, { tenured: true });

    // Create a tenured dependent string that will be processed next, and lose
    // its pointer to the original nursery base NB4. Which is fine for itself
    // since it can update its chars pointer before losing NB4, but any
    // dependencies processed later will no longer have a way of knowing their
    // original base.
    //
    // Initially, this is an extensible string with excess capacity. It will be
    // converted a dependent string during flattening, below.
    var TD3 = newString("MY YOUNGEST MEMORY IS OF A TOE, A GIANT BLUE TOE, IT MADE FUN OF ME INCESSANTLY BUT THAT DID NOT BOTHER ME IN THE LEAST. MY MOTHER WOULD HAVE BEEN HORRIFIED, BUT ",
        { tenured: true, capacity: 1000 });

    // A nursery dependent string to get messed up.
    var ND2 = newDependentString(TD3, 25, 106, { tenured: false });

    // Flatten a rope to convert TD3 from extensible to dependent.
    var suffix = "SHE WAS A GOOSE AND HAD ALREADY LAID THE EGG THAT CONTAINED ME SO SHE DID NOT ESPECIALLY CARE AND THOUGHT THAT IT WOULD BE GOOD TO BE TOUGHENED UP.";

    // Create a base NB4 that will be deduplicated to TB5 aka tenured(NB5).
    var NB4 = TD3 + suffix;
    ensureLinearString(NB4);

    // For debuggging:
    Math.sin(0, "ND2", ND2, "TD3", TD3, "NB4", NB4, "NB5", NB5, "TD6", TD6);

    var NB4_rep = this.stringRepresentation ? JSON.parse(stringRepresentation(NB4)) : null;

    // Clear some roots.
    rope = suffix = TD3 = NB4 = NB5 = "";

    gc();
    print(ND2);
    assertEq(ND2, "A TOE, A GIANT BLUE TOE, IT MADE FUN OF ME INCESSANTLY BUT THAT DID NOT BOTHER ME");
    // This only works because NB4 has the NON_DEDUP_BIT, and is an extensible
    // string so its data will not be allocated from the nursery.
    if (NB4_rep !== null) {
        assertEq(NB4_rep.flags.includes("NON_DEDUP_BIT"), true);
    }
}

with_dependent();
with_rope();
