// Test aborting an incremental GC in all possible states

if (!("gcstate" in this && "gczeal" in this && "abortgc" in this))
    quit();

function testAbort(zoneCount, objectCount, sliceCount, abortState)
{
    // Allocate objectCount objects in zoneCount zones and run a incremental
    // shrinking GC.

    var zones = [];
    for (var i = 0; i < zoneCount; i++) {
        var zone = newGlobal();
        evaluate("var objects; " +
                 "function makeObjectGraph(objectCount) { " +
                 "    objects = []; " +
                 "    for (var i = 0; i < objectCount; i++) " +
                 "        objects.push({i: i}); " +
                "}",
                 { global: zone });
        zone.makeObjectGraph(objectCount);
        zones.push(zone);
    }

    var didAbort = false;
    startgc(sliceCount, "shrinking");
    while (gcstate() !== "none") {
        var state = gcstate();
        if (state == abortState) {
            abortgc();
            didAbort = true;
            break;
        }

        gcslice(sliceCount);
    }

    assertEq(gcstate(), "none");
    if (abortState)
        assertEq(didAbort, true);

    return zones;
}

gczeal(0);
testAbort(10, 10000, 10000);
testAbort(10, 10000, 10000, "mark");
testAbort(10, 10000, 10000, "sweep");
testAbort(10, 10000, 10000, "compact");
