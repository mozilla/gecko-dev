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

// This case *does* happen in practice, in an actual web page. I'm not totally
// clear on how. But it's actually the simpler case where no deduplication is
// involved, just a nursery->tenured->nursery chain, which triggered an assert.
function no_dedupe() {
    // Add an object to the whole cell buffer so it can be used later to trace
    // the correct string first. The whole cell buffer is used surprisingly
    // little outside the JIT and strings, but a tenured WeakRef with a nursery
    // target will use it. Fortunately, WeakRefs are always tenured.
    var Tobj = new WeakRef(Object.create(null));

    // Tenured -> nursery edge, put in whole cell buffer.
    Tobj.name = newString("blah", { tenured: false });
    var NB4 = newString("diddle doodle dawdle dink. piddle poodle paddle pink. widdle woodle wattle wink.", { tenured: false });
    var TD3 = newDependentString(NB4, 0, 70, { tenured: true });
    var ND2 = newDependentString(TD3, 0, 60, { tenured: false, 'suppress-contraction': true });
    Tobj.name = ND2; // Make Tobj point to ND2 to process it first.
    NB4 = TD3 = null;

    // When tracing the whole cell buffer, Tobj will cause ND2 to be promoted
    // first. Later ND2 is promoted, and it has a base chain of ND2->TD3->NB4.
    // TD3, though in the whole cell buffer, has not yet been traced, so it
    // still points to the nursery version of NB4. As a result, when ND2 is
    // promoted to TD2, it will see NB4 as its base string, which still has the
    // original chars pointer. ND2 is able to force-promote NB4 to a tenured TB4
    // and recompute its chars as TD2.chars=(NB4.chars-ND2.chars)+TB4.chars. It
    // is difficult but not impossible to trigger this code path in practice.
    minorgc();
    return ND2;
}

function with_dependent(mallocChars) {
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

function atomref() {
    // Create a string too big to be inline if stored twoByte, but small enough
    // if stored latin1, and force it to be twoByte.
    var inlineIfLatin1 = newString("0123456789", { tenured: false, twoByte: true });

    // Make a tenured rope with a nursery child so it goes into the whole cell
    // buffer.
    var s = newRope(inlineIfLatin1, "abc", { nursery: false });

    // Make a toplevel rope node for flattening fun.
    var rope = newRope("....", s);

    // Flatten the toplevel, so `s` becomes a dependent string in the whole cell
    // buffer, pointing at the root.
    ensureLinearString(rope);

    // Convert the string to an AtomRef to an atom, deflated to latin1 so it is
    // inline.
    ({})[s] = true;

    // The whole cell buffer will trace `s`, which is an atomref (a type of
    // dependent string) that points to an inline atom.
    minorgc();
}

with_dependent();
with_rope();
atomref();
