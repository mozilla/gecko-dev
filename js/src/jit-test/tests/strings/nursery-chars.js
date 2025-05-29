function TTN() {
    let minors = this.currentgc ? currentgc()?.minorCount : undefined;
    let NB3 = newString("bleaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaggh!!!!! no no no no no no no no no no no no no no no no no", { tenured: false });
    if (this.stringRepresentation) {
        const NB3rep = JSON.parse(stringRepresentation(NB3));
        if (!NB3rep.charsInNursery) {
            printErr("Not testing what it is supposed to be testing");
            dumpStringRepresentation(NB3);
        }
    }
    let TD2 = newDependentString(NB3, 1, { tenured: true });
    let TD1 = newDependentString(TD2, 4, 56, { tenured: true, 'suppress-contraction': true });
    const TD1rep = this.stringRepresentation ? JSON.parse(stringRepresentation(TD1)) : null;
    NB3 = null;
    let wrong_situation = minors === undefined || currentgc().minorCount > minors;
    minorgc();
    print(TD1);
    assertEq(TD1, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaggh!!!!!");
    // Even with suppress-contraction, TD1 must point directly to NB3 as its base.
    if (wrong_situation) {
        printErr("unexpected minor GC or cannot determine, skipping whitebox tests");
    } else if (TD1rep !== null) {
        assertEq(TD1rep.base.base, undefined);
        assertEq(TD1rep.base.flags.includes("DEPENDED_ON_BIT"), true);
        assertEq(TD1rep.base.flags.includes("DEPENDENT_BIT"), false);
    }
}

TTN();

function TTTN() {
    let ext = newString("leafleafleafleafleafleafleaf", { capacity: 500 });
    let first = ext; // This will end up being the beginning of a long dependent chain.

    // Create a long dependent chain.
    let rope;
    for (let i = 0; i < 10; i++) {
        rope = newRope(ext, "y", { nursery: false });
        ensureLinearString(rope); // ext is now a dependent string pointing at rope
        [ext, rope] = [rope, ext];
    }

    // Add a final terminating link to the chain (the root base), but make it
    // nursery-allocated.
    rope = newRope(ext, "z", { nursery: true });
    ensureLinearString(rope);
    [ext, rope] = [rope, ext]

    // Sadly, the root base is nursery-allocated, but its chars are not. And
    // there is no way to make them nursery-allocated, because the whole
    // chain-making process requires an extensible string, and extensible string
    // data cannot be allocated in the nursery. So this dependent chain cannot
    // be used to try to create a tenured -> tenured -> nursery chars situation.
}

TTTN();

function TTTN2() {
    let s = newString("abcdefghijklmnopqrstuvwxyz.abcdefghijklmnopqrstuvwxyz", { tenured: false });

    let dep = s;
    for (let i = 0; i < 20; i++) {
        dep = dep.match(/.(.*)/)[1];
    }

    // Regexp matches also avoid lengthening the chain, so cannot be used either.
}

TTTN2();
